#include <string>

#include "check.h"
#include "fake_storage.h"
#include "summit/settings.h"

using namespace summit;

static SettingsSchema v1()
{
    SettingsSchema s;
    s.version = 1;
    s.defs = {{"ui/brightness", int64_t{80}, {}}, {"ui/theme", std::string("dark"), {"dark", "light"}}};
    return s;
}

// Same version, one key added: no bump needed.
static SettingsSchema v1_plus_key()
{
    SettingsSchema s = v1();
    s.defs.push_back({"ui/clock_seconds", true, {}});
    return s;
}

// v2: rename ui/theme -> ui/colour_theme (a rename needs a bump + migration).
static SettingsSchema v2()
{
    SettingsSchema s;
    s.version = 2;
    s.defs = {{"ui/brightness", int64_t{80}, {}},
              {"ui/colour_theme", std::string("dark"), {"dark", "light"}}};
    s.migrations = {{1, [](SettingsMap &m) {
                         auto it = m.find("ui/theme");
                         if (it != m.end()) {
                             m["ui/colour_theme"] = it->second;
                             m.erase(it);
                         }
                     }}};
    return s;
}

TEST(schema_validation)
{
    std::string err;
    CHECK(validate_schema(v1(), &err));
    CHECK(validate_schema(v2(), &err));
    SettingsSchema s = v2();
    s.migrations.clear();
    CHECK(!validate_schema(s, &err) && err == "missing migration");
    s = v1();
    s.defs.push_back({"ui/brightness", int64_t{1}, {}});
    CHECK(!validate_schema(s, &err));
    s = v1();
    s.defs.push_back({"_internal", true, {}});
    CHECK(!validate_schema(s, &err));
    s = v1();
    s.defs.push_back({"x", Value{}, {}});
    CHECK(!validate_schema(s, &err));
    s = v1();
    s.defs.push_back({"y", std::string("z"), {"a", "b"}});
    CHECK(!validate_schema(s, &err));
    s = v1();
    s.version = 2;
    s.migrations = {{1, [](SettingsMap &) {}}, {1, [](SettingsMap &) {}}};
    CHECK(!validate_schema(s, &err));
}

TEST(fresh_device_writes_defaults_with_version)
{
    FakeStorage st;
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::Fresh);
    CHECK(s.notice() == SettingsNotice::None && !s.read_only());
    CHECK(std::get<int64_t>(st.map().at(Settings::kVersionKey)) == 1);
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
}

TEST(set_validates_persists_and_publishes)
{
    FakeStorage st;
    EventBus bus;
    Settings s(v1(), st, &bus);
    s.load();
    std::string seen;
    bus.subscribe("settings/ui/theme", [&](const std::string &, const Value &v) { seen = std::get<std::string>(v); });
    CHECK(s.set("ui/theme", std::string("light")) == SetResult::Ok);
    CHECK(s.set("ui/theme", std::string("light")) == SetResult::Unchanged);
    CHECK(s.set("ui/theme", std::string("pink")) == SetResult::NotAChoice);
    CHECK(s.set("ui/theme", int64_t{1}) == SetResult::WrongType);
    CHECK(s.set("ui/nope", true) == SetResult::UnknownKey);
    CHECK(std::get<std::string>(st.map().at("ui/theme")) == "light");
    bus.dispatch();
    CHECK(seen == "light");
}

TEST(save_failure_rolls_back)
{
    FakeStorage st;
    Settings s(v1(), st);
    s.load();
    st.fail_write = true;
    CHECK(s.set("ui/brightness", int64_t{10}) == SetResult::StorageError);
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
}

TEST(older_reader_round_trips_unknown_keys_unchanged)
{
    // Newer firmware (same schema version, one added key) writes settings...
    FakeStorage st;
    Settings newer(v1_plus_key(), st);
    CHECK(newer.load() == LoadResult::Fresh);
    CHECK(newer.set("ui/clock_seconds", false) == SetResult::Ok);
    SettingsMap written = st.map();

    // ...older firmware without that key loads, changes something else, saves.
    Settings older(v1(), st);
    CHECK(older.load() == LoadResult::Current);
    CHECK(!older.read_only());
    CHECK(type_of(older.get("ui/clock_seconds")) == ValueType::None);
    CHECK(older.set("ui/brightness", int64_t{40}) == SetResult::Ok);
    SettingsMap after = st.map();
    CHECK(after.at("ui/clock_seconds") == written.at("ui/clock_seconds"));
    CHECK(std::get<int64_t>(after.at("ui/brightness")) == 40);

    // Newer firmware sees its value intact.
    Settings again(v1_plus_key(), st);
    CHECK(again.load() == LoadResult::Current);
    CHECK(std::get<bool>(again.get("ui/clock_seconds")) == false);
}

TEST(newer_reader_fills_added_key_with_default)
{
    FakeStorage st;
    Settings older(v1(), st);
    older.load();
    Settings newer(v1_plus_key(), st);
    CHECK(newer.load() == LoadResult::Current);
    CHECK(std::get<bool>(newer.get("ui/clock_seconds")) == true);
}

TEST(uninterpretable_value_is_preserved_until_user_sets_key)
{
    FakeStorage st;
    st.put({{Settings::kVersionKey, int64_t{1}}, {"ui/theme", std::string("neon")}});
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::Current);
    CHECK(std::get<std::string>(s.get("ui/theme")) == "dark");  // default in use
    s.set("ui/brightness", int64_t{10});
    CHECK(std::get<std::string>(st.map().at("ui/theme")) == "neon");  // preserved
    CHECK(s.set("ui/theme", std::string("dark")) == SetResult::Ok);   // explicit choice wins
    CHECK(std::get<std::string>(st.map().at("ui/theme")) == "dark");
}

TEST(migrates_v1_to_v2_and_saves)
{
    FakeStorage st;
    Settings old(v1(), st);
    old.load();
    old.set("ui/theme", std::string("light"));
    old.set("ui/brightness", int64_t{55});

    Settings s(v2(), st);
    CHECK(s.load() == LoadResult::Migrated);
    CHECK(s.stored_version() == 2);
    CHECK(std::get<std::string>(s.get("ui/colour_theme")) == "light");
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 55);
    SettingsMap m = st.map();
    CHECK(std::get<int64_t>(m.at(Settings::kVersionKey)) == 2);
    CHECK(m.count("ui/theme") == 0);
}

TEST(newer_schema_is_not_written_and_raises_notice)
{
    FakeStorage st;
    Settings newer(v2(), st);
    newer.load();
    newer.set("ui/brightness", int64_t{20});
    std::string before = st.blob;
    int writes = st.writes;

    Settings s(v1(), st);  // older firmware after a rollback
    CHECK(s.load() == LoadResult::NewerSchema);
    CHECK(s.notice() == SettingsNotice::NewerVersion && s.read_only());
    CHECK(std::string(notice_message(s.notice())) ==
          "Settings are from a newer version - update or reset");
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 20);  // compatible key still read
    CHECK(s.set("ui/brightness", int64_t{90}) == SetResult::ReadOnly);
    CHECK(st.writes == writes && st.blob == before);
}

TEST(reset_backs_up_old_blob_then_writes_defaults)
{
    FakeStorage st;
    Settings newer(v2(), st);
    newer.load();
    newer.set("ui/brightness", int64_t{20});
    std::string old_blob = st.blob;

    EventBus bus;
    Settings s(v1(), st, &bus);
    s.load();
    bool reset_seen = false;
    bus.subscribe("settings/reset", [&](const std::string &, const Value &) { reset_seen = true; });
    CHECK(s.stored_blob() == old_blob);  // exportable before reset
    CHECK(s.reset() == ResetResult::Ok);
    CHECK(st.backup == old_blob);
    CHECK(s.notice() == SettingsNotice::None && !s.read_only());
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
    CHECK(std::get<int64_t>(st.map().at(Settings::kVersionKey)) == 1);
    CHECK(s.set("ui/brightness", int64_t{30}) == SetResult::Ok);
    bus.dispatch();
    CHECK(reset_seen);
}

TEST(reset_refuses_if_backup_fails)
{
    FakeStorage st;
    st.put({{Settings::kVersionKey, int64_t{9}}});
    std::string old_blob = st.blob;
    Settings s(v1(), st);
    s.load();
    st.fail_backup = true;
    CHECK(s.reset() == ResetResult::BackupFailed);
    CHECK(st.blob == old_blob && s.read_only());
}

TEST(no_migration_path_raises_notice)
{
    FakeStorage st;
    st.put({{"ui/brightness", int64_t{1}}});  // unversioned
    Settings s(v2(), st);
    CHECK(s.load() == LoadResult::MigrationMissing);
    CHECK(s.notice() == SettingsNotice::NoMigration && st.writes == 0);
}

TEST(corrupt_blob_runs_on_defaults_keeps_raw_and_can_reset)
{
    FakeStorage st;
    st.blob = std::string("garbage\x01\x02", 9);
    st.has_blob = true;
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::Unreadable);
    CHECK(s.notice() == SettingsNotice::Unreadable);
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
    CHECK(s.stored_blob() == st.blob);           // raw kept for export
    CHECK(s.set("ui/brightness", int64_t{5}) == SetResult::ReadOnly);
    CHECK(st.writes == 0);
    CHECK(s.reset() == ResetResult::Ok);
    CHECK(st.backup == std::string("garbage\x01\x02", 9));
    CHECK(s.notice() == SettingsNotice::None);
}

TEST(read_error_runs_on_defaults_and_can_reset)
{
    FakeStorage st;
    st.fail_read = true;
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::Unreadable);
    CHECK(s.stored_blob().empty());
    CHECK(s.reset() == ResetResult::Ok);  // nothing to back up
    CHECK(!st.has_backup && s.notice() == SettingsNotice::None);
}

TEST(codec_round_trip)
{
    SettingsMap m = {{"a", true},
                     {"b", int64_t{-1234567890123}},
                     {"c", 3.25},
                     {"d", std::string("h\0llo", 5)},
                     {"e", Value{}},
                     {Settings::kVersionKey, int64_t{2}}};
    std::string blob;
    CHECK(encode_settings(m, blob));
    SettingsMap back;
    CHECK(decode_settings(blob, back));
    CHECK(back == m);
}

TEST(codec_rejects_malformed)
{
    SettingsMap m = {{"k", std::string("value")}};
    std::string blob, bad;
    encode_settings(m, blob);
    SettingsMap out = {{"keep", true}};
    for (size_t n = 0; n < blob.size(); ++n) CHECK(!decode_settings(blob.substr(0, n), out));
    CHECK(!decode_settings(blob + "x", out));
    bad = blob;
    bad[0] = 'X';
    CHECK(!decode_settings(bad, out));
    bad = blob;
    bad[4 + 2 + 1 + 1] = 9;  // unknown type tag
    CHECK(!decode_settings(bad, out));
    CHECK(out.count("keep") == 1);  // untouched on failure
    CHECK(!encode_settings({{std::string(256, 'k'), true}}, blob));
    CHECK(!encode_settings({{"s", std::string(70000, 'x')}}, blob));
}

int main()
{
    RUN(schema_validation);
    RUN(fresh_device_writes_defaults_with_version);
    RUN(set_validates_persists_and_publishes);
    RUN(save_failure_rolls_back);
    RUN(older_reader_round_trips_unknown_keys_unchanged);
    RUN(newer_reader_fills_added_key_with_default);
    RUN(uninterpretable_value_is_preserved_until_user_sets_key);
    RUN(migrates_v1_to_v2_and_saves);
    RUN(newer_schema_is_not_written_and_raises_notice);
    RUN(reset_backs_up_old_blob_then_writes_defaults);
    RUN(reset_refuses_if_backup_fails);
    RUN(no_migration_path_raises_notice);
    RUN(corrupt_blob_runs_on_defaults_keeps_raw_and_can_reset);
    RUN(read_error_runs_on_defaults_and_can_reset);
    RUN(codec_round_trip);
    RUN(codec_rejects_malformed);
    return report("settings");
}
