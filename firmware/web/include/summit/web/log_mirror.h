// The console log kept on the SD card, in rolling files, so it survives a reset, a crash or a power cut (the copy in RAM
// does not survive a power loss). Pure logic on the Storage interface: host-tested in tests/host/test_log_mirror.cpp.
//
// Files: /logs/log-000001.txt, log-000002.txt, ... A file grows to kMaxFileBytes, then the next one starts. A file is
// deleted only once kKeepNewer newer files exist (so the newest two stay), and a restart does not start a new file: the
// boot banner in the text marks it. Nothing in the web UI can clear or delete the log.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "summit/storage.h"

namespace summit::web {

constexpr size_t kLogFileMaxBytes = 256 * 1024;
constexpr int kLogKeepNewer = 2;

class LogMirror {
public:
    explicit LogMirror(Storage &storage, std::string dir = "/logs", size_t max_file_bytes = kLogFileMaxBytes)
        : storage_(storage), dir_(std::move(dir)), max_file_(max_file_bytes) {}

    // Finds the newest file (or starts the first one). Returns the last `tail_bytes` of what it holds: the log of the
    // previous boot(s), to show at the top of the Log tab. Call once, when the card is present.
    std::string open(size_t tail_bytes);
    // Appends log text to the current file, rolls to a new file when it would pass the limit, then deletes old files.
    bool append(const std::string &text);

    std::string current_path() const { return path_for(seq_); }
    std::vector<int> sequences();  // numbers of the files in the folder, oldest first

private:
    std::string path_for(int seq) const;
    void trim();

    Storage &storage_;
    std::string dir_;
    size_t max_file_;
    int seq_ = 0;
    size_t size_ = 0;
};

// When to write the pending text to the card: at once (within 2 s) after a warning or error line, otherwise once a minute.
class LogFlushPolicy {
public:
    bool due(int64_t now_ms, bool has_text, bool important) const
    {
        if (!has_text) return false;
        return now_ms - last_ms_ >= (important ? 2000 : 60000);
    }
    void flushed(int64_t now_ms) { last_ms_ = now_ms; }

private:
    int64_t last_ms_ = -60000;  // so the first ordinary write is due at once (right after the card mounts)
};

// True for an ESP log line of level W or E ("W (123) tag: ..." after any colour escape).
bool log_line_is_important(const char *line, size_t len);

}  // namespace summit::web
