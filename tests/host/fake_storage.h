#pragma once
#include <string>

#include "summit/settings.h"

struct FakeStorage : summit::SettingsStorage {
    std::string blob, backup;
    bool has_blob = false, has_backup = false;
    bool fail_read = false, fail_write = false, fail_backup = false;
    int writes = 0;

    summit::ReadStatus read(std::string &out) override
    {
        if (fail_read) return summit::ReadStatus::Error;
        if (!has_blob) return summit::ReadStatus::Empty;
        out = blob;
        return summit::ReadStatus::Ok;
    }
    bool write(const std::string &b) override
    {
        if (fail_write) return false;
        blob = b;
        has_blob = true;
        ++writes;
        return true;
    }
    bool write_backup(const std::string &b) override
    {
        if (fail_backup) return false;
        backup = b;
        has_backup = true;
        return true;
    }
    summit::ReadStatus read_backup(std::string &out) override
    {
        if (!has_backup) return summit::ReadStatus::Empty;
        out = backup;
        return summit::ReadStatus::Ok;
    }

    // Test helpers
    void put(const summit::SettingsMap &m)
    {
        summit::encode_settings(m, blob);
        has_blob = true;
    }
    summit::SettingsMap map() const
    {
        summit::SettingsMap m;
        summit::decode_settings(blob, m);
        return m;
    }
};
