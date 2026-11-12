#include "summit/modules.h"

namespace summit {

std::vector<std::string> missing_requirements(const ModuleRequirements &req, const ModuleEnv &env)
{
    std::vector<std::string> out;
    if (req.sd_card && !env.sd_present) {
        out.push_back("Insert a microSD card");
        return out;  // free space is unknown without a card
    }
    if (req.min_free_mb > 0 && env.sd_free_mb < req.min_free_mb) {
        out.push_back("Free up space on the microSD card: " + std::to_string(req.min_free_mb) + " MB needed, " +
                      std::to_string(env.sd_free_mb) + " MB free");
    }
    return out;
}

std::string requirements_text(const ModuleRequirements &req)
{
    if (!req.sd_card && req.min_free_mb <= 0) return "";
    std::string s = "Needs a microSD card";
    if (req.min_free_mb > 0) s += " with " + std::to_string(req.min_free_mb) + " MB free";
    return s;
}

bool ModuleManager::add(const ModuleManifest &m, std::string *error)
{
    auto fail = [error](const std::string &msg) {
        if (error) *error = msg;
        return false;
    };
    if (!valid_topic(m.id) || m.id.find('/') != std::string::npos) return fail("invalid module id: " + m.id);
    if (m.name.empty()) return fail("module '" + m.id + "' has no name");
    if (m.requires_.min_free_mb < 0) return fail("module '" + m.id + "': negative min_free_mb");
    if (m.requires_.min_free_mb > 0 && !m.requires_.sd_card)
        return fail("module '" + m.id + "': min_free_mb needs sd_card");
    if (find(m.id)) return fail("duplicate module id: " + m.id);
    modules_.push_back(m);
    return true;
}

const ModuleManifest *ModuleManager::find(const std::string &id) const
{
    for (const ModuleManifest &m : modules_)
        if (m.id == id) return &m;
    return nullptr;
}

std::vector<SettingDef> ModuleManager::setting_defs() const
{
    std::vector<SettingDef> out;
    for (const ModuleManifest &m : modules_) out.push_back(SettingDef(installed_key(m.id), false));
    return out;
}

bool ModuleManager::installed(const Settings &settings, const std::string &id) const
{
    Value v = settings.get(installed_key(id));
    const bool *b = std::get_if<bool>(&v);
    return b && *b;
}

ModuleResult ModuleManager::install(Settings &settings, const std::string &id, const ModuleEnv &env,
                                    std::vector<std::string> *missing) const
{
    const ModuleManifest *m = find(id);
    if (!m) return ModuleResult::UnknownModule;
    if (installed(settings, id)) return ModuleResult::AlreadyInstalled;
    std::vector<std::string> miss = missing_requirements(m->requires_, env);
    if (!miss.empty()) {
        if (missing) *missing = miss;
        return ModuleResult::MissingRequirements;
    }
    return settings.set(installed_key(id), true) == SetResult::Ok ? ModuleResult::Ok : ModuleResult::SettingsError;
}

ModuleResult ModuleManager::remove(Settings &settings, const std::string &id) const
{
    if (!find(id)) return ModuleResult::UnknownModule;
    if (!installed(settings, id)) return ModuleResult::NotInstalled;
    return settings.set(installed_key(id), false) == SetResult::Ok ? ModuleResult::Ok : ModuleResult::SettingsError;
}

std::vector<std::string> ModuleManager::hidden_panels(const Settings &settings) const
{
    std::vector<std::string> out;
    for (const ModuleManifest &m : modules_)
        if (!m.panel_id.empty() && !installed(settings, m.id)) out.push_back(m.panel_id);
    return out;
}

}  // namespace summit
