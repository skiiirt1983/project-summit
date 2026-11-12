// Removable storage (the microSD card) behind an interface, so everything above it is
// tested on the host with a local folder as the "SD card". Paths are absolute within
// the card: "/recipes/pancakes/recipe.json".
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "summit/state_store.h"

namespace summit {

struct DirEntry {
    std::string name;
    bool is_dir = false;
    int64_t size = 0;
};

// "/a/b.txt": absolute, no empty / "." / ".." segments, no backslashes or control
// characters, at most 255 bytes. "/" is the card root.
bool valid_storage_path(const std::string &path);

class Storage {
public:
    virtual ~Storage() = default;
    virtual bool present() = 0;     // card inserted and mounted
    virtual int64_t free_mb() = 0;  // 0 when not present
    virtual bool exists(const std::string &path) = 0;
    virtual bool read(const std::string &path, std::string &out, size_t max_bytes = 4 * 1024 * 1024) = 0;
    // Writes via a temporary file and a rename, creating parent folders.
    virtual bool write(const std::string &path, const std::string &data) = 0;
    virtual bool list(const std::string &dir, std::vector<DirEntry> &out) = 0;  // sorted by name
    virtual bool make_dir(const std::string &dir) = 0;                          // with parents
};

// Storage on a mounted POSIX file system: a local folder on the host / simulator,
// the FAT mount point ("/sdcard") on the device.
class PosixStorage : public Storage {
public:
    // free_mb: device passes its own (ESP-IDF has no statvfs); host default uses statvfs.
    explicit PosixStorage(std::string root, std::function<int64_t()> free_mb = nullptr);
    void set_present(bool present) { present_ = present; }  // mount state (device) / test hook

    bool present() override;
    int64_t free_mb() override;
    bool exists(const std::string &path) override;
    bool read(const std::string &path, std::string &out, size_t max_bytes = 4 * 1024 * 1024) override;
    bool write(const std::string &path, const std::string &data) override;
    bool list(const std::string &dir, std::vector<DirEntry> &out) override;
    bool make_dir(const std::string &dir) override;
    // Host file name for a card path ("" if invalid), e.g. for LVGL's file system driver.
    std::string host_path(const std::string &path) const;

private:
    std::string root_;
    std::function<int64_t()> free_mb_;
    bool present_ = true;
};

// Publishes storage/sd_present and storage/sd_free_mb (call on mount changes and
// every few seconds).
void publish_storage_state(Storage &storage, StateStore &state);

}  // namespace summit
