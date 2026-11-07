// Hard rule 4: the product settings schema is locked per version.
// Adding a key (with a default) is allowed without a bump. Removing or renaming a
// key, changing its type, or dropping one of its choices fails here unless the
// version is bumped. On a bump: add the new version's locked keys below, keep the
// old ones, add a Migration in system_settings.cpp and a migration test.
#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/system_settings.h"

using namespace summit;

struct Locked {
    std::string key;
    ValueType type;
    std::vector<std::string> choices;  // may grow, never shrink
};

// Index = version - 1.
static const std::vector<std::vector<Locked>> kLocked = {
    // v1
    {{"update/channel", ValueType::String, {"stable", "beta", "alpha"}},
     {"ui/rotation_lock", ValueType::String, {"off", "landscape", "portrait"}},
     {"ui/page_order", ValueType::String, {}},
     {"display/brightness", ValueType::Int, {}},
     {"audio/volume", ValueType::Int, {}},
     {"system/dnd", ValueType::Bool, {}}},
};

TEST(schema_valid_and_compatible_with_locked_version)
{
    SettingsSchema s = system_settings_schema();
    std::string err;
    CHECK(validate_schema(s, &err));
    CHECK(s.version == kLocked.size());
    for (const Locked &l : kLocked[s.version - 1]) {
        const SettingDef *d = nullptr;
        for (const auto &x : s.defs) if (x.key == l.key) d = &x;
        CHECK(d != nullptr);
        if (!d) {
            std::fprintf(stderr, "    removed/renamed without a version bump: %s\n", l.key.c_str());
            continue;
        }
        CHECK(type_of(d->default_value) == l.type);
        for (const auto &c : l.choices) {
            bool kept = false;
            for (const auto &x : d->choices) kept = kept || x == c;
            CHECK(kept);
        }
    }
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
    RUN(schema_valid_and_compatible_with_locked_version);
    RUN(fresh_load_of_product_schema);
    return report("settings schema");
}
