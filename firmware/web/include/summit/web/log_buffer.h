// The console log kept in memory so the web UI can show it, copy it and download it.
// Pure logic (host-tested in tests/host/test_web_api.cpp); the device fills it from the ESP log output
// (esp/log_capture.cpp), the simulator with sample lines.
#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "summit/web/api.h"

namespace summit::web {

class LogBuffer {
public:
    explicit LogBuffer(size_t capacity = 48 * 1024) : capacity_(capacity) {}
    // Appends raw console text. ANSI colour codes are dropped (they are noise when pasted); when the buffer is
    // full the oldest text goes, always at a line boundary.
    void append(const char *data, size_t len);
    void append(const std::string &s) { append(s.data(), s.size()); }
    std::string text() const;                 // everything, oldest first
    std::string tail(size_t lines) const;     // the last `lines` lines
    void clear();                              // tests only: not reachable from the web UI
    // Puts older text in front (the previous boot's log read from the SD card). Trimmed from its oldest end to fit.
    void prepend(const std::string &older);
    size_t size() const;
    uint64_t total_bytes() const;             // ever appended (so a viewer can tell something new arrived)

private:
    mutable std::mutex mu_;
    std::string buf_;
    size_t capacity_;
    uint64_t total_ = 0;
    bool in_escape_ = false;  // an ANSI escape sequence split across two append() calls
};

// GET /api/log[?lines=N][&download=1]   the log as plain text (download -> summit-log.txt)
// (There is no way to clear the log from the web UI: it is also kept on the SD card and is evidence.)
class LogApi : public ApiExtension {
public:
    explicit LogApi(LogBuffer &log) : log_(log) {}
    bool handle(const Request &req, Response &out) override;
    std::unique_ptr<UploadSink> upload(const Request &, int64_t, Response &) override { return nullptr; }

private:
    LogBuffer &log_;
};

}  // namespace summit::web
