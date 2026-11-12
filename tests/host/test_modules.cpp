#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/modules.h"
#include "summit/system_settings.h"

using namespace summit;

static ModuleManifest recipes()
{
    ModuleManifest m;
    m.id = "recipes";
    m.name = "Recipe reader";
    m.description = "Cook from your own recipes.";
    m.panel_id = "recipes";
    m.requires_.sd_card = true;
    m.requires_.min_free_mb = 200;
    return m;
}

TEST(requirements_explain_what_is_missing)
{
    ModuleRequirements r = recipes().requires_;
    CHECK((missing_requirements(r, {false, 0}) == std::vector<std::string>{"Insert a microSD card"}));
    auto low = missing_requirements(r, {true, 150});
    CHECK(low.size() == 1 && low[0] == "Free up space on the microSD card: 200 MB needed, 150 MB free");
    CHECK(missing_requirements(r, {true, 200}).empty());
    CHECK(missing_requirements(ModuleRequirements{}, {false, 0}).empty());
    CHECK(requirements_text(r) == "Needs a microSD card with 200 MB free");
    CHECK(requirements_text(ModuleRequirements{}).empty());
}

TEST(manifest_validation)
{
    ModuleManager mm;
    std::string err;
    CHECK(mm.add(recipes(), &err));
    CHECK(!mm.add(recipes(), &err) && err == "duplicate module id: recipes");
    ModuleManifest bad = recipes();
    bad.id = "Bad Id";
    CHECK(!mm.add(bad, &err));
    bad = recipes();
    bad.id = "x";
    bad.name = "";
    CHECK(!mm.add(bad, &err));
    bad = recipes();
    bad.id = "y";
    bad.requires_.sd_card = false;
    CHECK(!mm.add(bad, &err) && err.find("needs sd_card") != std::string::npos);
}

struct Rig {
    ModuleManager mm;
    FakeStorage st;
    std::unique_ptr<Settings> settings;
    Rig()
    {
        mm.add(recipes());
        SettingsSchema s = system_settings_schema();
        for (const auto &d : mm.setting_defs()) s.defs.push_back(d);
        std::string err;
        CHECK(validate_schema(s, &err));  // additive keys, still schema v1
        CHECK(s.version == 1);
        settings = std::make_unique<Settings>(s, st);
        settings->load();
    }
};

TEST(modules_start_uninstalled_and_install_checks_requirements)
{
    Rig r;
    CHECK(!r.mm.installed(*r.settings, "recipes"));
    CHECK((r.mm.hidden_panels(*r.settings) == std::vector<std::string>{"recipes"}));
    std::vector<std::string> missing;
    CHECK(r.mm.install(*r.settings, "recipes", {false, 0}, &missing) == ModuleResult::MissingRequirements);
    CHECK(missing.size() == 1 && !r.mm.installed(*r.settings, "recipes"));
    CHECK(r.mm.install(*r.settings, "recipes", {true, 4000}) == ModuleResult::Ok);
    CHECK(r.mm.installed(*r.settings, "recipes") && r.mm.hidden_panels(*r.settings).empty());
    CHECK(std::get<bool>(r.st.map().at("modules/recipes/installed")) == true);  // persisted
    CHECK(r.mm.install(*r.settings, "recipes", {true, 4000}) == ModuleResult::AlreadyInstalled);
    CHECK(r.mm.install(*r.settings, "nope", {true, 4000}) == ModuleResult::UnknownModule);
}

TEST(remove_hides_and_card_removal_does_not_uninstall)
{
    Rig r;
    r.mm.install(*r.settings, "recipes", {true, 4000});
    // SD card pulled later: still installed (the panel shows "Insert the SD card").
    CHECK(r.mm.installed(*r.settings, "recipes"));
    CHECK(r.mm.remove(*r.settings, "recipes") == ModuleResult::Ok);
    CHECK(!r.mm.installed(*r.settings, "recipes"));
    CHECK(r.mm.remove(*r.settings, "recipes") == ModuleResult::NotInstalled);
    CHECK(r.mm.remove(*r.settings, "nope") == ModuleResult::UnknownModule);
}

int main()
{
    RUN(requirements_explain_what_is_missing);
    RUN(manifest_validation);
    RUN(modules_start_uninstalled_and_install_checks_requirements);
    RUN(remove_hides_and_card_removal_does_not_uninstall);
    return report("modules");
}
