// Settings persistence in NVS: the whole map as one blob, so a save is atomic.
#pragma once
#include "summit/settings.h"

class NvsSettingsStorage : public summit::SettingsStorage {
public:
    explicit NvsSettingsStorage(const char *nvs_namespace = "settings") : ns_(nvs_namespace) {}
    bool load(summit::SettingsMap &out) override;
    bool save(const summit::SettingsMap &values) override;

private:
    const char *ns_;
};
