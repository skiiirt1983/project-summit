#include "summit/storage.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ESP_PLATFORM
#include <sys/statvfs.h>
#endif

namespace summit {

bool valid_storage_path(const std::string &path)
{
    if (path.empty() || path.size() > 255 || path[0] != '/') return false;
    if (path == "/") return true;
    size_t start = 1;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        const std::string seg = path.substr(start, end - start);
        if (seg.empty() || seg == "." || seg == "..") return false;
        for (unsigned char c : seg)
            if (c < 0x20 || c == 0x7F || c == '\\' || c == ':') return false;
        start = end + 1;
    }
    return true;
}

PosixStorage::PosixStorage(std::string root, std::function<int64_t()> free_mb)
    : root_(std::move(root)), free_mb_(std::move(free_mb))
{
    while (root_.size() > 1 && root_.back() == '/') root_.pop_back();
}

std::string PosixStorage::host_path(const std::string &path) const
{
    if (!valid_storage_path(path)) return "";
    return path == "/" ? root_ : root_ + path;
}

bool PosixStorage::present()
{
    struct stat st;
    return present_ && stat(root_.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int64_t PosixStorage::free_mb()
{
    if (!present()) return 0;
    if (free_mb_) return free_mb_();
#ifndef ESP_PLATFORM
    struct statvfs v;
    if (statvfs(root_.c_str(), &v) == 0)
        return static_cast<int64_t>(v.f_bavail) * static_cast<int64_t>(v.f_frsize) / (1024 * 1024);
#endif
    return 0;
}

bool PosixStorage::exists(const std::string &path)
{
    struct stat st;
    const std::string p = host_path(path);
    return present() && !p.empty() && stat(p.c_str(), &st) == 0;
}

bool PosixStorage::read(const std::string &path, std::string &out, size_t max_bytes)
{
    const std::string p = host_path(path);
    if (!present() || p.empty()) return false;
    struct stat st;
    if (stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || static_cast<size_t>(st.st_size) > max_bytes) return false;
    FILE *f = fopen(p.c_str(), "rb");
    if (!f) return false;
    std::string data(static_cast<size_t>(st.st_size), '\0');
    const size_t n = data.empty() ? 0 : fread(&data[0], 1, data.size(), f);
    fclose(f);
    if (n != data.size()) return false;
    out = std::move(data);
    return true;
}

bool PosixStorage::make_dir(const std::string &dir)
{
    if (!present() || !valid_storage_path(dir)) return false;
    if (dir == "/") return true;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = dir.find('/', pos + 1);
        const std::string p = root_ + dir.substr(0, pos);
        struct stat st;
        if (stat(p.c_str(), &st) == 0) {
            if (!S_ISDIR(st.st_mode)) return false;
        } else if (mkdir(p.c_str(), 0755) != 0) {
            return false;
        }
    }
    return true;
}

bool PosixStorage::write(const std::string &path, const std::string &data)
{
    const std::string p = host_path(path);
    if (!present() || p.empty() || path == "/") return false;
    if (!make_dir(path.substr(0, std::max<size_t>(1, path.rfind('/'))))) return false;
    const std::string tmp = p + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool ok = (data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size()) && fflush(f) == 0;
    fclose(f);
    if (!ok) {
        ::remove(tmp.c_str());
        return false;
    }
    // FAT can't rename over an existing file: remove the old one first. (Not atomic on
    // FAT; a power cut here leaves the .tmp file, which readers ignore.)
    ::remove(p.c_str());
    if (rename(tmp.c_str(), p.c_str()) != 0) {
        ::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool PosixStorage::list(const std::string &dir, std::vector<DirEntry> &out)
{
    const std::string p = host_path(dir);
    if (!present() || p.empty()) return false;
    DIR *d = opendir(p.c_str());
    if (!d) return false;
    std::vector<DirEntry> entries;
    while (dirent *e = readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == ".." || (name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0)) continue;
        struct stat st;
        if (stat((p + "/" + name).c_str(), &st) != 0) continue;
        entries.push_back({name, S_ISDIR(st.st_mode), S_ISDIR(st.st_mode) ? 0 : static_cast<int64_t>(st.st_size)});
    }
    closedir(d);
    std::sort(entries.begin(), entries.end(), [](const DirEntry &a, const DirEntry &b) { return a.name < b.name; });
    out = std::move(entries);
    return true;
}

void publish_storage_state(Storage &storage, StateStore &state)
{
    const bool present = storage.present();
    state.set("storage/sd_present", present);
    state.set("storage/sd_free_mb", present ? storage.free_mb() : int64_t{0});
}

}  // namespace summit
