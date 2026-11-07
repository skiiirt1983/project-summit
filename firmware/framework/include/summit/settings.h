// Schema-versioned settings (CLAUDE.md hard rule 4).
//
// Every change to the set of keys, their types or meaning bumps
// SettingsSchema::version and adds a Migration from the previous version,
// plus a host test. Adding a key with a default still needs the bump and a
// (possibly empty) migration so the history stays complete.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "summit/event_bus.h"
#include "summit/value.h"

namespace summit {

using SettingsMap = std::map<std::string, Value>;

struct SettingDef {
    std::string key;                   // topic syntax, must not start with '_'
    Value default_value;               // also fixes the type
    std::vector<std::string> choices;  // optional: allowed values for a string setting
};

struct Migration {
    uint32_t from;                              // migrates from -> from + 1
    std::function<void(SettingsMap &)> apply;   // edits stored values in place
};

struct SettingsSchema {
    uint32_t version = 1;
    std::vector<SettingDef> defs;
    std::vector<Migration> migrations;
};

// Checks keys, types, choices and that migrations cover 1..version-1 exactly once.
bool validate_schema(const SettingsSchema &schema, std::string *error);

// Persistence backend: whole-map load/save so a save is atomic.
class SettingsStorage {
public:
    virtual ~SettingsStorage() = default;
    // Nothing stored yet: return true with an empty map. Unreadable: return false.
    virtual bool load(SettingsMap &out) = 0;
    virtual bool save(const SettingsMap &values) = 0;
};

enum class LoadResult {
    Fresh,             // nothing stored; defaults written
    Current,           // stored schema == firmware schema
    Migrated,          // older schema migrated and saved
    NewerSchema,       // stored by newer firmware (e.g. after a rollback): read-only
    MigrationMissing,  // no migration path: defaults, read-only
    StorageError,      // could not read storage: defaults, read-only
    InvalidSchema,     // firmware bug: schema failed validation
};

enum class SetResult { Ok, Unchanged, UnknownKey, WrongType, NotAChoice, ReadOnly, StorageError };

class Settings {
public:
    static constexpr const char *kVersionKey = "_schema_version";

    // Changes are published on `bus` (if given) as "settings/<key>".
    Settings(SettingsSchema schema, SettingsStorage &storage, EventBus *bus = nullptr);

    LoadResult load();

    // Unknown key -> std::monostate.
    Value get(const std::string &key) const;
    SetResult set(const std::string &key, Value value);

    // Read-only means values work in memory but are never written, so data
    // written by newer firmware survives until that firmware returns.
    bool read_only() const;
    uint32_t schema_version() const { return schema_.version; }
    uint32_t stored_version() const;
    const SettingsSchema &schema() const { return schema_; }
    SettingsMap values() const;

private:
    const SettingDef *def(const std::string &key) const;
    void overlay_known(const SettingsMap &stored);
    SettingsMap to_storage() const;

    const SettingsSchema schema_;
    SettingsStorage &storage_;
    EventBus *bus_;
    mutable std::mutex mu_;
    SettingsMap values_;
    bool read_only_ = true;
    uint32_t stored_version_ = 0;
};

// Binary blob codec used by the NVS backend. decode() rejects any malformed input.
// encode() fails for a key over 255 bytes or a string over 65535 bytes.
bool encode_settings(const SettingsMap &values, std::string &out);
bool decode_settings(const std::string &blob, SettingsMap &out);

}  // namespace summit
