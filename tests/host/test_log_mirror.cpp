#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "check.h"
#include "summit/storage.h"
#include "summit/web/log_buffer.h"
#include "summit/web/log_mirror.h"

using namespace summit;
using namespace summit::web;

static std::string tmpdir()
{
    char tmpl[] = "/tmp/summit-logmirror-XXXXXX";
    return mkdtemp(tmpl);
}

static std::string text_of(PosixStorage &sd, const LogMirror &m)
{
    std::string s;
    sd.read(m.current_path(), s);
    return s;
}

TEST(appends_and_survives_a_restart)
{
    PosixStorage sd(tmpdir());
    sd.set_present(true);
    {
        LogMirror m(sd);
        CHECK(m.open(1000).empty());                      // nothing before the first boot
        CHECK(m.append("I (1) boot one\nW (2) disconnected\n"));
        CHECK(m.append("I (3) more\n"));
    }
    LogMirror m2(sd);                                      // the next boot (after a reset or a power cut)
    CHECK(m2.open(1000) == "I (1) boot one\nW (2) disconnected\nI (3) more\n");
    CHECK(m2.append("I (1) boot two\n"));
    CHECK(text_of(sd, m2) == "I (1) boot one\nW (2) disconnected\nI (3) more\nI (1) boot two\n");  // the same file
    CHECK(m2.sequences().size() == 1);
}

TEST(tail_is_cut_at_a_line_start)
{
    PosixStorage sd(tmpdir());
    sd.set_present(true);
    LogMirror m(sd);
    m.open(0);
    m.append("aaaa\nbbbb\ncccc\n");
    LogMirror m2(sd);
    CHECK(m2.open(8) == "cccc\n");                         // never half a line
}

TEST(rolls_over_and_deletes_a_file_once_two_newer_exist)
{
    PosixStorage sd(tmpdir());
    sd.set_present(true);
    LogMirror m(sd, "/logs", 100);                         // 100-byte files
    m.open(0);
    const std::string chunk(60, 'x');
    CHECK(m.append(chunk + "\n"));                         // file 1: 61 bytes
    CHECK(m.sequences() == std::vector<int>({1}));
    CHECK(m.append(chunk + "\n"));                         // would pass 100: file 2
    CHECK(m.sequences() == std::vector<int>({1, 2}));      // file 1 has one newer file: kept
    CHECK(m.append(chunk + "\n"));                         // file 3: file 1 now has two newer ones
    CHECK(m.sequences() == std::vector<int>({2, 3}));
    CHECK(m.append(chunk + "\n"));                         // file 4: file 2 goes
    CHECK(m.sequences() == std::vector<int>({3, 4}));
    for (int i = 0; i < 10; ++i) CHECK(m.append(chunk + "\n"));
    CHECK(m.sequences().size() == 2);                      // never more than the newest two (plus none older)
}

TEST(a_restart_does_not_start_a_new_file_so_crash_loops_do_not_evict_history)
{
    PosixStorage sd(tmpdir());
    sd.set_present(true);
    for (int boot = 0; boot < 8; ++boot) {
        LogMirror m(sd);
        m.open(100);
        m.append("I (1) summit: Project Summit booting\n");
    }
    LogMirror m(sd);
    CHECK(m.sequences().size() == 1);
}

TEST(the_log_in_ram_cannot_be_cleared_through_the_api_and_older_text_goes_in_front)
{
    LogBuffer log(64);
    log.append("this boot line 1\nthis boot line 2\n");
    log.prepend("older line A\nolder line B\n");
    CHECK(log.text() == "older line A\nolder line B\nthis boot line 1\nthis boot line 2\n");
    LogBuffer small(40);
    small.append("new line\n");
    small.prepend("0123456789\nabcdefghij\nklmnopqrst\n");  // too much: the oldest part is dropped, at a line start
    CHECK(small.size() <= 40 && small.text().rfind("new line\n") == small.text().size() - 9);
    CHECK(small.text().find("0123456789") == std::string::npos);
}

TEST(flush_policy_and_important_lines)
{
    LogFlushPolicy p;
    CHECK(!p.due(5000, false, true));                      // nothing to write
    CHECK(p.due(5000, true, true));                        // a warning: within 2 s
    p.flushed(5000);
    CHECK(!p.due(6000, true, true) && p.due(7000, true, true));
    CHECK(!p.due(30000, true, false) && p.due(65000, true, false));  // ordinary lines: once a minute
    const std::string w = "W (123) net: disconnected (reason 200)\n";
    const std::string e = "\033[0;31mE (5) x: bad\033[0m\n";
    const std::string i = "I (123) net: connected\n";
    CHECK(log_line_is_important(w.data(), w.size()) && log_line_is_important(e.data(), e.size()));
    CHECK(!log_line_is_important(i.data(), i.size()) && !log_line_is_important("", 0));
}

int main()
{
    RUN(appends_and_survives_a_restart);
    RUN(tail_is_cut_at_a_line_start);
    RUN(rolls_over_and_deletes_a_file_once_two_newer_exist);
    RUN(a_restart_does_not_start_a_new_file_so_crash_loops_do_not_evict_history);
    RUN(the_log_in_ram_cannot_be_cleared_through_the_api_and_older_text_goes_in_front);
    RUN(flush_policy_and_important_lines);
    return report("log mirror");
}
