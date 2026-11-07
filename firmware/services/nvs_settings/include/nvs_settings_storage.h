// Settings persistence in NVS: the whole settings blob under one key (atomic
// via nvs_commit), plus the pre-reset backup blob for export.
#pragma once
#include "summit/settings.h"

class NvsSettingsStorage : public summit::SettingsStorage {
public:
    explicit NvsSettingsStorage(const char *nvs_namespace = "settings") : ns_(nvs_namespace) {}
    summit::ReadStatus read(std::string &blob) override { return read_key("v", blob); }
    bool write(const std::string &blob) override { return write_key("v", blob); }
    bool write_backup(const std::string &blob) override { return write_key("backup", blob); }
    summit::ReadStatus read_backup(std::string &blob) override { return read_key("backup", blob); }

private:
    summit::ReadStatus read_key(const char *key, std::string &blob);
    bool write_key(const char *key, const std::string &blob);
    const char *ns_;
};
