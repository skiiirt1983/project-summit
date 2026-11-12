// Modules (v1 "installable"): features that ship in the firmware but start
// uninstalled. Install = setting modules/<id>/installed (additive, no schema bump)
// and the module's panel joins the pages. See ARCHITECTURE.md "Modules". Pure logic.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/settings.h"

namespace summit {

struct ModuleRequirements {
    bool sd_card = false;
    int64_t min_free_mb = 0;  // on the SD card; 0 = no minimum
};

struct ModuleManifest {
    std::string id;           // topic-segment syntax, e.g. "recipes"
    std::string name;
    std::string icon;
    std::string description;  // one short sentence
    std::string panel_id;     // panel this module adds to the pages (may be empty)
    ModuleRequirements requires_;
};

// What the device has right now (from storage/sd_present, storage/sd_free_mb).
struct ModuleEnv {
    bool sd_present = false;
    int64_t sd_free_mb = 0;
};

// Human-readable list of what is missing; empty = requirements met.
std::vector<std::string> missing_requirements(const ModuleRequirements &req, const ModuleEnv &env);
// One-line summary of a module's requirements for the Modules screen ("" if none).
std::string requirements_text(const ModuleRequirements &req);

enum class ModuleResult { Ok, AlreadyInstalled, NotInstalled, UnknownModule, MissingRequirements, SettingsError };

class ModuleManager {
public:
    // Rejects an invalid or duplicate manifest.
    bool add(const ModuleManifest &m, std::string *error = nullptr);
    const std::vector<ModuleManifest> &modules() const { return modules_; }
    const ModuleManifest *find(const std::string &id) const;

    // Settings keys for every module, to append to the schema before Settings::load().
    std::vector<SettingDef> setting_defs() const;
    static std::string installed_key(const std::string &id) { return "modules/" + id + "/installed"; }

    bool installed(const Settings &settings, const std::string &id) const;
    // Checks requirements first; `missing` gets the reasons on MissingRequirements.
    ModuleResult install(Settings &settings, const std::string &id, const ModuleEnv &env,
                         std::vector<std::string> *missing = nullptr) const;
    // Hides the module. Never deletes its data.
    ModuleResult remove(Settings &settings, const std::string &id) const;

    // Panels that must not appear in the pages: those of uninstalled modules.
    std::vector<std::string> hidden_panels(const Settings &settings) const;

private:
    std::vector<ModuleManifest> modules_;
};

}  // namespace summit
