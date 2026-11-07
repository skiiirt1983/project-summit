// Hard rule 4: the product settings schema is locked per version.
// Changing keys, types, defaults or choices without bumping the version fails here.
// On a bump: add the new version's expected keys below, keep the old ones, add a
// Migration in system_settings.cpp and a migration test.
#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/system_settings.h"

using namespace summit;

static std::string describe(const SettingDef &d)
{
    std::string s = d.key + ":" + std::to_string(static_cast<int>(type_of(d.default_value))) + "=";
    if (auto v = std::get_if<std::string>(&d.default_value)) s += *v;
    if (auto v = std::get_if<int64_t>(&d.default_value)) s += std::to_string(*v);
    if (auto v = std::get_if<bool>(&d.default_value)) s += *v ? "true" : "false";
    for (const auto &c : d.choices) s += "|" + c;
    return s;
}

static const std::vector<std::vector<std::string>> kLocked = {
    // v1
    {"update/channel:4=stable|stable|beta|alpha",
     "ui/rotation_lock:4=off|off|landscape|portrait"},
};

TEST(schema_valid_and_locked)
{
    SettingsSchema s = system_settings_schema();
    std::string err;
    CHECK(validate_schema(s, &err));
    CHECK(s.version == kLocked.size());
    std::vector<std::string> now;
    for (const auto &d : s.defs) now.push_back(describe(d));
    CHECK(now == kLocked[s.version - 1]);
    if (now != kLocked[s.version - 1])
        for (auto &line : now) std::fprintf(stderr, "    actual: %s\n", line.c_str());
}

TEST(fresh_load_of_product_schema)
{
    FakeStorage st;
    Settings s(system_settings_schema(), st);
    CHECK(s.load() == LoadResult::Fresh);
    CHECK(std::get<std::string>(s.get("update/channel")) == "stable");
}

int main()
{
    RUN(schema_valid_and_locked);
    RUN(fresh_load_of_product_schema);
    return report("settings schema");
}
