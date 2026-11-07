#include "summit/settings.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace summit {

bool validate_schema(const SettingsSchema &schema, std::string *error)
{
    auto fail = [error](const std::string &msg) {
        if (error) *error = msg;
        return false;
    };
    if (schema.version < 1) return fail("schema version must be >= 1");
    std::set<std::string> keys;
    for (const SettingDef &d : schema.defs) {
        if (!valid_topic(d.key) || d.key[0] == '_') return fail("invalid key: " + d.key);
        if (!keys.insert(d.key).second) return fail("duplicate key: " + d.key);
        if (type_of(d.default_value) == ValueType::None) return fail("no default: " + d.key);
        if (!d.choices.empty()) {
            const std::string *s = std::get_if<std::string>(&d.default_value);
            if (!s) return fail("choices need a string setting: " + d.key);
            if (std::find(d.choices.begin(), d.choices.end(), *s) == d.choices.end())
                return fail("default not in choices: " + d.key);
        }
    }
    std::set<uint32_t> froms;
    for (const Migration &m : schema.migrations) {
        if (m.from < 1 || m.from >= schema.version) return fail("migration outside 1..version-1");
        if (!m.apply) return fail("empty migration");
        if (!froms.insert(m.from).second) return fail("duplicate migration");
    }
    if (froms.size() != schema.version - 1) return fail("missing migration");
    return true;
}

Settings::Settings(SettingsSchema schema, SettingsStorage &storage, EventBus *bus)
    : schema_(std::move(schema)), storage_(storage), bus_(bus)
{
    for (const SettingDef &d : schema_.defs) values_[d.key] = d.default_value;
}

const SettingDef *Settings::def(const std::string &key) const
{
    for (const SettingDef &d : schema_.defs) {
        if (d.key == key) return &d;
    }
    return nullptr;
}

void Settings::overlay_known(const SettingsMap &stored)
{
    for (const auto &kv : stored) {
        const SettingDef *d = def(kv.first);
        if (!d || type_of(kv.second) != type_of(d->default_value)) continue;
        if (!d->choices.empty() &&
            std::find(d->choices.begin(), d->choices.end(),
                      std::get<std::string>(kv.second)) == d->choices.end())
            continue;
        values_[kv.first] = kv.second;
    }
}

SettingsMap Settings::to_storage() const
{
    SettingsMap out = values_;
    out[kVersionKey] = static_cast<int64_t>(schema_.version);
    return out;
}

LoadResult Settings::load()
{
    std::lock_guard<std::mutex> lock(mu_);
    read_only_ = true;
    for (const SettingDef &d : schema_.defs) values_[d.key] = d.default_value;
    if (!validate_schema(schema_, nullptr)) return LoadResult::InvalidSchema;

    SettingsMap stored;
    if (!storage_.load(stored)) return LoadResult::StorageError;

    if (stored.empty()) {
        stored_version_ = 0;
        if (!storage_.save(to_storage())) return LoadResult::StorageError;
        read_only_ = false;
        return LoadResult::Fresh;
    }

    // Data without a version predates versioning: treat as version 0 (no migration -> safe).
    uint32_t version = 0;
    auto it = stored.find(kVersionKey);
    if (it != stored.end()) {
        const int64_t *v = std::get_if<int64_t>(&it->second);
        if (v && *v > 0 && *v <= INT32_MAX) version = static_cast<uint32_t>(*v);
        stored.erase(it);
    }
    stored_version_ = version;

    if (version > schema_.version) {
        overlay_known(stored);
        return LoadResult::NewerSchema;
    }
    if (version == schema_.version) {
        overlay_known(stored);
        read_only_ = false;
        return LoadResult::Current;
    }
    for (uint32_t v = version; v < schema_.version; ++v) {
        auto m = std::find_if(schema_.migrations.begin(), schema_.migrations.end(),
                              [v](const Migration &x) { return x.from == v; });
        if (m == schema_.migrations.end()) return LoadResult::MigrationMissing;
        m->apply(stored);
    }
    overlay_known(stored);
    if (!storage_.save(to_storage())) return LoadResult::StorageError;
    read_only_ = false;
    return LoadResult::Migrated;
}

Value Settings::get(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = values_.find(key);
    return it == values_.end() ? Value{} : it->second;
}

SetResult Settings::set(const std::string &key, Value value)
{
    {
        std::lock_guard<std::mutex> lock(mu_);
        const SettingDef *d = def(key);
        if (!d) return SetResult::UnknownKey;
        if (type_of(value) != type_of(d->default_value)) return SetResult::WrongType;
        if (!d->choices.empty() &&
            std::find(d->choices.begin(), d->choices.end(), std::get<std::string>(value)) ==
                d->choices.end())
            return SetResult::NotAChoice;
        if (read_only_) return SetResult::ReadOnly;
        if (values_[key] == value) return SetResult::Unchanged;
        Value old = values_[key];
        values_[key] = value;
        if (!storage_.save(to_storage())) {
            values_[key] = old;
            return SetResult::StorageError;
        }
    }
    if (bus_) bus_->publish("settings/" + key, std::move(value));
    return SetResult::Ok;
}

bool Settings::read_only() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return read_only_;
}

uint32_t Settings::stored_version() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return stored_version_;
}

SettingsMap Settings::values() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return values_;
}

// ---- codec -----------------------------------------------------------------
// "SST1" | u16 count | count x (u8 key_len, key, u8 type, payload)
// payload: bool u8 | int i64 LE | double 8 bytes LE | string u16 len + bytes

namespace {

constexpr char kMagic[4] = {'S', 'S', 'T', '1'};

void put_le(std::string &out, uint64_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

bool get_le(const std::string &in, size_t &pos, int bytes, uint64_t &v)
{
    if (in.size() - pos < static_cast<size_t>(bytes)) return false;
    v = 0;
    for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(static_cast<uint8_t>(in[pos + i])) << (8 * i);
    pos += bytes;
    return true;
}

}  // namespace

bool encode_settings(const SettingsMap &values, std::string &result)
{
    if (values.size() > 0xFFFF) return false;
    std::string out(kMagic, 4);
    put_le(out, values.size(), 2);
    for (const auto &kv : values) {
        if (kv.first.empty() || kv.first.size() > 0xFF) return false;
        put_le(out, kv.first.size(), 1);
        out += kv.first;
        put_le(out, static_cast<uint64_t>(type_of(kv.second)), 1);
        switch (type_of(kv.second)) {
        case ValueType::None: break;
        case ValueType::Bool: put_le(out, std::get<bool>(kv.second) ? 1 : 0, 1); break;
        case ValueType::Int: put_le(out, static_cast<uint64_t>(std::get<int64_t>(kv.second)), 8); break;
        case ValueType::Double: {
            uint64_t bits;
            double d = std::get<double>(kv.second);
            std::memcpy(&bits, &d, sizeof bits);
            put_le(out, bits, 8);
            break;
        }
        case ValueType::String: {
            const std::string &s = std::get<std::string>(kv.second);
            if (s.size() > 0xFFFF) return false;
            put_le(out, s.size(), 2);
            out += s;
            break;
        }
        }
    }
    result = std::move(out);
    return true;
}

bool decode_settings(const std::string &blob, SettingsMap &out)
{
    SettingsMap result;
    size_t pos = 0;
    uint64_t count;
    if (blob.size() < 4 || blob.compare(0, 4, kMagic, 4) != 0) return false;
    pos = 4;
    if (!get_le(blob, pos, 2, count)) return false;
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t key_len, type, v;
        if (!get_le(blob, pos, 1, key_len) || blob.size() - pos < key_len) return false;
        std::string key = blob.substr(pos, key_len);
        pos += key_len;
        if (key.empty() || result.count(key)) return false;
        if (!get_le(blob, pos, 1, type)) return false;
        Value value;
        switch (static_cast<ValueType>(type)) {
        case ValueType::None: break;
        case ValueType::Bool:
            if (!get_le(blob, pos, 1, v) || v > 1) return false;
            value = v == 1;
            break;
        case ValueType::Int:
            if (!get_le(blob, pos, 8, v)) return false;
            value = static_cast<int64_t>(v);
            break;
        case ValueType::Double: {
            if (!get_le(blob, pos, 8, v)) return false;
            double d;
            std::memcpy(&d, &v, sizeof d);
            value = d;
            break;
        }
        case ValueType::String:
            if (!get_le(blob, pos, 2, v) || blob.size() - pos < v) return false;
            value = blob.substr(pos, v);
            pos += v;
            break;
        default: return false;
        }
        result[key] = std::move(value);
    }
    if (pos != blob.size()) return false;
    out = std::move(result);
    return true;
}

}  // namespace summit
