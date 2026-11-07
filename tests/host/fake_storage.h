#pragma once
#include "summit/settings.h"

struct FakeStorage : summit::SettingsStorage {
    summit::SettingsMap data;
    bool fail_load = false, fail_save = false;
    int saves = 0;
    bool load(summit::SettingsMap &out) override
    {
        if (fail_load) return false;
        out = data;
        return true;
    }
    bool save(const summit::SettingsMap &v) override
    {
        if (fail_save) return false;
        data = v;
        ++saves;
        return true;
    }
};
