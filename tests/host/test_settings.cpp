#include <cstring>
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

// v2: rename ui/theme -> ui/colour_theme, add ui/night_mode.
static SettingsSchema v2()
{
    SettingsSchema s;
    s.version = 2;
    s.defs = {{"ui/brightness", int64_t{80}, {}},
              {"ui/colour_theme", std::string("dark"), {"dark", "light"}},
              {"ui/night_mode", true, {}}};
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
    CHECK(!s.read_only());
    CHECK(std::get<int64_t>(st.data.at(Settings::kVersionKey)) == 1);
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
    CHECK(std::get<std::string>(st.data.at("ui/theme")) == "light");
    bus.dispatch();
    CHECK(seen == "light");
}

TEST(save_failure_rolls_back)
{
    FakeStorage st;
    Settings s(v1(), st);
    s.load();
    st.fail_save = true;
    CHECK(s.set("ui/brightness", int64_t{10}) == SetResult::StorageError);
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
}

TEST(current_schema_keeps_values_and_drops_bad_ones)
{
    FakeStorage st;
    st.data = {{Settings::kVersionKey, int64_t{1}},
               {"ui/brightness", int64_t{30}},
               {"ui/theme", std::string("neon")},  // not a choice -> default
               {"ui/unknown", true}};               // unknown -> ignored
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::Current);
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 30);
    CHECK(std::get<std::string>(s.get("ui/theme")) == "dark");
    CHECK(type_of(s.get("ui/unknown")) == ValueType::None);
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
    CHECK(s.stored_version() == 1);
    CHECK(std::get<std::string>(s.get("ui/colour_theme")) == "light");
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 55);
    CHECK(std::get<bool>(s.get("ui/night_mode")) == true);
    CHECK(std::get<int64_t>(st.data.at(Settings::kVersionKey)) == 2);
    CHECK(st.data.count("ui/theme") == 0);
}

TEST(newer_schema_after_rollback_is_read_only_and_untouched)
{
    FakeStorage st;
    Settings newer(v2(), st);
    newer.load();
    newer.set("ui/brightness", int64_t{20});
    SettingsMap before = st.data;
    int saves = st.saves;

    Settings s(v1(), st);  // older firmware after a rollback
    CHECK(s.load() == LoadResult::NewerSchema);
    CHECK(s.read_only());
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 20);  // shared key still read
    CHECK(s.set("ui/brightness", int64_t{90}) == SetResult::ReadOnly);
    CHECK(st.saves == saves && st.data == before);
}

TEST(storage_error_is_read_only_defaults)
{
    FakeStorage st;
    st.fail_load = true;
    Settings s(v1(), st);
    CHECK(s.load() == LoadResult::StorageError);
    CHECK(s.read_only());
    CHECK(std::get<int64_t>(s.get("ui/brightness")) == 80);
    CHECK(st.saves == 0);
}

TEST(unversioned_data_without_migration_is_read_only)
{
    FakeStorage st;
    st.data = {{"ui/brightness", int64_t{1}}};
    Settings s(v2(), st);
    CHECK(s.load() == LoadResult::MigrationMissing);
    CHECK(s.read_only() && st.saves == 0);
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
    RUN(current_schema_keeps_values_and_drops_bad_ones);
    RUN(migrates_v1_to_v2_and_saves);
    RUN(newer_schema_after_rollback_is_read_only_and_untouched);
    RUN(storage_error_is_read_only_defaults);
    RUN(unversioned_data_without_migration_is_read_only);
    RUN(codec_round_trip);
    RUN(codec_rejects_malformed);
    return report("settings");
}
