#include "summit/web/log_buffer.h"

#include <cstdlib>

namespace summit::web {

void LogBuffer::append(const char *data, size_t len)
{
    std::lock_guard<std::mutex> lock(mu_);
    for (size_t i = 0; i < len; ++i) {
        const char c = data[i];
        if (in_escape_) {  // ESC [ ... final letter
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) in_escape_ = false;
            continue;
        }
        if (c == 0x1B) {
            in_escape_ = true;
            continue;
        }
        if (c == '\r') continue;
        buf_.push_back(c);
        ++total_;
    }
    if (buf_.size() > capacity_) {
        size_t cut = buf_.size() - capacity_;
        const size_t nl = buf_.find('\n', cut);
        cut = nl == std::string::npos ? buf_.size() : nl + 1;
        buf_.erase(0, cut);
    }
}

std::string LogBuffer::text() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return buf_;
}

std::string LogBuffer::tail(size_t lines) const
{
    std::lock_guard<std::mutex> lock(mu_);
    if (lines == 0) return std::string();
    size_t pos = buf_.size();
    if (pos && buf_[pos - 1] == '\n') --pos;  // the final newline ends the last line
    size_t seen = 0;
    while (pos > 0) {
        --pos;
        if (buf_[pos] == '\n' && ++seen == lines) return buf_.substr(pos + 1);
    }
    return buf_;
}

void LogBuffer::prepend(const std::string &older)
{
    std::lock_guard<std::mutex> lock(mu_);
    std::string text = older;
    if (text.size() + buf_.size() > capacity_) {
        const size_t room = capacity_ > buf_.size() ? capacity_ - buf_.size() : 0;
        text = text.size() > room ? text.substr(text.size() - room) : text;
        const size_t nl = text.find('\n');  // start at a line boundary
        text = nl == std::string::npos ? std::string() : text.substr(nl + 1);
    }
    buf_.insert(0, text);
}

void LogBuffer::clear()
{
    std::lock_guard<std::mutex> lock(mu_);
    buf_.clear();
}

size_t LogBuffer::size() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return buf_.size();
}

uint64_t LogBuffer::total_bytes() const
{
    std::lock_guard<std::mutex> lock(mu_);
    return total_;
}

bool LogApi::handle(const Request &req, Response &out)
{
    if (req.path == "/api/log" && req.method == "GET") {
        size_t lines = 0;
        bool download = false;
        size_t at = 0;
        while (at <= req.query.size()) {  // tiny query parser: lines=N&download=1
            size_t amp = req.query.find('&', at);
            if (amp == std::string::npos) amp = req.query.size();
            const std::string kv = req.query.substr(at, amp - at);
            if (kv.rfind("lines=", 0) == 0) lines = static_cast<size_t>(std::strtoul(kv.c_str() + 6, nullptr, 10));
            if (kv == "download=1") download = true;
            at = amp + 1;
        }
        out.status = 200;
        out.content_type = "text/plain; charset=utf-8";
        out.body = lines ? log_.tail(lines) : log_.text();
        if (download) out.filename = "summit-log.txt";
        return true;
    }
    return false;
}

}  // namespace summit::web
