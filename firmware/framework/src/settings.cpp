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

const char *notice_message(SettingsNotice notice)
{
    switch (notice) {
    case SettingsNotice::None: return "";
    case SettingsNotice::NewerVersion: return "Settings are from a newer version \u2014 update or reset";
    case SettingsNotice::NoMigration:
        return "Settings are from a version this firmware can't upgrade \u2014 update or reset";
    case SettingsNotice::Unreadable: return "Settings could not be read \u2014 using defaults. Export or reset";
    }
    return "";
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

bool Settings::accepts(const SettingDef &d, const Value &v) const
{
    if (type_of(v) != type_of(d.default_value)) return false;
    if (d.choices.empty()) return true;
    return std::find(d.choices.begin(), d.choices.end(), std::get<std::string>(v)) != d.choices.end();
}

// Usable values go to values_; everything else is kept verbatim for write-back.
void Settings::apply_stored(const SettingsMap &stored)
{
    for (const auto &kv : stored) {
        const SettingDef *d = def(kv.first);
        if (d && accepts(*d, kv.second)) {
            values_[kv.first] = kv.second;
        } else {
            passthrough_[kv.first] = kv.second;
        }
    }
}

bool Settings::persist()
{
    SettingsMap out = values_;
    for (const auto &kv : passthrough_) out[kv.first] = kv.second;
    out[kVersionKey] = static_cast<int64_t>(schema_.version);
    std::string blob;
    if (!encode_settings(out, blob) || !storage_.write(blob)) return false;
    stored_blob_ = std::move(blob);
    stored_version_ = schema_.version;
    return true;
}

LoadResult Settings::load()
{
    std::lock_guard<std::mutex> lock(mu_);
    values_.clear();
    passthrough_.clear();
    stored_blob_.clear();
    stored_version_ = 0;
    for (const SettingDef &d : schema_.defs) values_[d.key] = d.default_value;
    notice_ = SettingsNotice::Unreadable;
    if (!validate_schema(schema_, nullptr)) return LoadResult::InvalidSchema;

    std::string blob;
    ReadStatus rs = storage_.read(blob);
    if (rs == ReadStatus::Error) return LoadResult::Unreadable;
    if (rs == ReadStatus::Empty) {
        notice_ = SettingsNotice::None;
        if (!persist()) {
            notice_ = SettingsNotice::Unreadable;
            return LoadResult::Unreadable;
        }
        return LoadResult::Fresh;
    }
    stored_blob_ = blob;
    SettingsMap stored;
    if (!decode_settings(blob, stored)) return LoadResult::Unreadable;

    // Data without a version predates versioning: version 0 (no migration -> notice).
    uint32_t version = 0;
    auto it = stored.find(kVersionKey);
    if (it != stored.end()) {
        const int64_t *v = std::get_if<int64_t>(&it->second);
        if (v && *v > 0 && *v <= INT32_MAX) version = static_cast<uint32_t>(*v);
        stored.erase(it);
    }
    stored_version_ = version;

    if (version > schema_.version) {
        apply_stored(stored);
        notice_ = SettingsNotice::NewerVersion;
        return LoadResult::NewerSchema;
    }
    if (version == schema_.version) {
        apply_stored(stored);
        notice_ = SettingsNotice::None;
        return LoadResult::Current;
    }
    for (uint32_t v = version; v < schema_.version; ++v) {
        auto m = std::find_if(schema_.migrations.begin(), schema_.migrations.end(),
                              [v](const Migration &x) { return x.from == v; });
        if (m == schema_.migrations.end()) {
            notice_ = SettingsNotice::NoMigration;
            return LoadResult::MigrationMissing;
        }
        m->apply(stored);
    }
    apply_stored(stored);
    notice_ = SettingsNotice::None;
    if (!persist()) {
        notice_ = SettingsNotice::Unreadable;
        return LoadResult::Unreadable;
    }
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
        if (!accepts(*d, value)) return SetResult::NotAChoice;
        if (notice_ != SettingsNotice::None) return SetResult::ReadOnly;
        if (values_[key] == value && passthrough_.count(key) == 0) return SetResult::Unchanged;
        Value old = values_[key];
        SettingsMap old_pass = passthrough_;
        values_[key] = value;
        passthrough_.erase(key);  // the user's value replaces whatever was stored
        if (!persist()) {
            values_[key] = old;
            passthrough_ = old_pass;
            return SetResult::StorageError;
        }
    }
    if (bus_) bus_->publish("settings/" + key, std::move(value));
    return SetResult::Ok;
}

ResetResult Settings::reset()
{
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!stored_blob_.empty() && !storage_.write_backup(stored_blob_))
            return ResetResult::BackupFailed;
        SettingsMap old_values = values_, old_pass = passthrough_;
        SettingsNotice old_notice = notice_;
        for (const SettingDef &d : schema_.defs) values_[d.key] = d.default_value;
        passthrough_.clear();
        notice_ = SettingsNotice::None;
        if (!persist()) {
            values_ = old_values;
            passthrough_ = old_pass;
            notice_ = old_notice;
            return ResetResult::WriteFailed;
        }
    }
    if (bus_) bus_->publish("settings/reset", true);
    return ResetResult::Ok;
}

SettingsNotice Settings::notice() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return notice_;
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

std::string Settings::stored_blob() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return stored_blob_;
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
