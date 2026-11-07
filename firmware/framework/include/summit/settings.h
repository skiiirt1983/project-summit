// Schema-versioned settings (CLAUDE.md hard rule 4).
//
// Versioning rule:
// - Adding a key with a default does NOT bump the schema version. Firmware that
//   does not know a key ignores it and preserves it unchanged on write.
// - Renaming or removing a key, or changing its type or meaning, bumps
//   SettingsSchema::version and needs a Migration (from -> from + 1) and a host test.
//
// Problem states never silently overwrite stored data:
// - Stored by newer firmware, or no migration path: values are read where
//   compatible, nothing is written, and a notice is raised.
// - Unreadable or corrupt: defaults in memory, raw blob kept for export, notice raised.
// Leaving a problem state takes an explicit reset(), which first saves the old
// blob as a backup for export.
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

enum class ReadStatus { Ok, Empty, Error };

// Persistence backend. Works on raw blobs so a corrupt blob can be kept and exported.
class SettingsStorage {
public:
    virtual ~SettingsStorage() = default;
    virtual ReadStatus read(std::string &blob) = 0;
    virtual bool write(const std::string &blob) = 0;  // must be atomic
    // Keeps the pre-reset blob so it can still be exported after a reset.
    virtual bool write_backup(const std::string &blob) = 0;
    virtual ReadStatus read_backup(std::string &blob) = 0;
};

enum class LoadResult {
    Fresh,             // nothing stored; defaults written
    Current,           // same schema version (may contain keys this firmware doesn't know)
    Migrated,          // older schema migrated and saved
    NewerSchema,       // stored by newer firmware: not written, notice raised
    MigrationMissing,  // older schema without a migration path: not written, notice raised
    Unreadable,        // read error or corrupt blob: defaults in memory, notice raised
    InvalidSchema,     // firmware bug: schema failed validation
};

// Shown on the device and in the web UI until resolved.
enum class SettingsNotice { None, NewerVersion, NoMigration, Unreadable };
const char *notice_message(SettingsNotice notice);

enum class SetResult { Ok, Unchanged, UnknownKey, WrongType, NotAChoice, ReadOnly, StorageError };
enum class ResetResult { Ok, BackupFailed, WriteFailed };

class Settings {
public:
    static constexpr const char *kVersionKey = "_schema_version";

    // Changes are published on `bus` (if given) as "settings/<key>";
    // a reset publishes "settings/reset".
    Settings(SettingsSchema schema, SettingsStorage &storage, EventBus *bus = nullptr);

    LoadResult load();

    // Unknown key -> std::monostate.
    Value get(const std::string &key) const;
    // Persists immediately. Returns ReadOnly while a notice is active.
    SetResult set(const std::string &key, Value value);

    // Explicit user action only. Saves the current stored blob as a backup,
    // then writes defaults and clears the notice.
    ResetResult reset();

    SettingsNotice notice() const;
    bool read_only() const { return notice() != SettingsNotice::None; }
    uint32_t schema_version() const { return schema_.version; }
    uint32_t stored_version() const;
    const SettingsSchema &schema() const { return schema_; }
    SettingsMap values() const;

    // The blob as stored (raw, even if corrupt) for settings export.
    std::string stored_blob() const;

private:
    const SettingDef *def(const std::string &key) const;
    bool accepts(const SettingDef &d, const Value &v) const;
    void apply_stored(const SettingsMap &stored);
    bool persist();  // mu_ held

    const SettingsSchema schema_;
    SettingsStorage &storage_;
    EventBus *bus_;
    mutable std::mutex mu_;
    SettingsMap values_;        // every schema key
    SettingsMap passthrough_;   // stored entries this firmware can't use: written back unchanged
    std::string stored_blob_;
    SettingsNotice notice_ = SettingsNotice::None;
    uint32_t stored_version_ = 0;
};

// Blob codec. encode() fails for a key over 255 bytes or a string over 65535
// bytes; decode() rejects any malformed input and leaves `out` untouched.
bool encode_settings(const SettingsMap &values, std::string &out);
bool decode_settings(const std::string &blob, SettingsMap &out);

}  // namespace summit
