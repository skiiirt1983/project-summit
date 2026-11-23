#include "summit/web/log_mirror.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace summit::web {

std::string LogMirror::path_for(int seq) const
{
    char name[32];
    snprintf(name, sizeof name, "/log-%06d.txt", seq);
    return dir_ + name;
}

std::vector<int> LogMirror::sequences()
{
    std::vector<int> out;
    std::vector<DirEntry> entries;
    if (!storage_.list(dir_, entries)) return out;
    for (const DirEntry &e : entries) {
        int n = 0;
        char tail = 0;
        if (!e.is_dir && e.name.size() == 14 && e.name.compare(0, 4, "log-") == 0 && e.name.compare(10, 4, ".txt") == 0 &&
            sscanf(e.name.c_str(), "log-%6d.t%c", &n, &tail) == 2 && n > 0)
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string LogMirror::open(size_t tail_bytes)
{
    const std::vector<int> seqs = sequences();
    seq_ = seqs.empty() ? 1 : seqs.back();
    const int64_t sz = storage_.size(path_for(seq_));
    size_ = sz > 0 ? static_cast<size_t>(sz) : 0;
    std::string tail;
    if (size_ == 0) return tail;
    // The previous boot may have been cut off in the newest file; if it is short, also read the file before it.
    std::string text;
    storage_.read(path_for(seq_), text, kLogFileMaxBytes * 2);
    if (text.size() < tail_bytes && seqs.size() >= 2) {
        std::string before;
        if (storage_.read(path_for(seqs[seqs.size() - 2]), before, kLogFileMaxBytes * 2)) text = before + text;
    }
    if (text.size() > tail_bytes) {
        text = text.substr(text.size() - tail_bytes);
        const size_t nl = text.find('\n');  // from a line start
        text = nl == std::string::npos ? std::string() : text.substr(nl + 1);
    }
    return text;
}

void LogMirror::trim()
{
    const std::vector<int> seqs = sequences();
    // seqs[i] has (size - 1 - i) newer files: delete it once that reaches kLogKeepNewer.
    for (size_t i = 0; i < seqs.size(); ++i)
        if (static_cast<int>(seqs.size() - 1 - i) >= kLogKeepNewer) storage_.remove_file(path_for(seqs[i]));
}

bool LogMirror::append(const std::string &text)
{
    if (text.empty()) return true;
    if (seq_ == 0) seq_ = 1;
    if (size_ > 0 && size_ + text.size() > max_file_) {  // roll over
        ++seq_;
        size_ = 0;
        const bool ok = storage_.append(path_for(seq_), text);
        if (ok) size_ = text.size();
        trim();
        return ok;
    }
    if (!storage_.append(path_for(seq_), text)) return false;
    size_ += text.size();
    return true;
}

bool log_line_is_important(const char *line, size_t len)
{
    size_t i = 0;
    while (i < len && (line[i] == '\033' || line[i] == ' ')) {  // skip a colour escape such as ESC[0;33m
        if (line[i] == '\033') {
            while (i < len && line[i] != 'm') ++i;
        }
        ++i;
    }
    return i + 2 < len && (line[i] == 'W' || line[i] == 'E') && line[i + 1] == ' ' && line[i + 2] == '(';
}

}  // namespace summit::web
