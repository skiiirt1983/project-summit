#include <string>

#include "check.h"
#include "summit/alarms.h"
#include "summit/stopwatch.h"
#include "summit/timers.h"

using namespace summit;

namespace {
// Local seconds for 2026-11-23 (a Monday) at hh:mm:ss, plus whole days.
constexpr int64_t kMon = 1795392000;  // 2026-11-23 00:00:00 (checked by the weekday test below)
int64_t at(int day, int h, int m, int s = 0) { return kMon + static_cast<int64_t>(day) * 86400 + h * 3600 + m * 60 + s; }
}  // namespace

TEST(stopwatch_counts_up_and_pauses)
{
    Stopwatch w;
    CHECK(!w.running() && w.elapsed_ms(5000) == 0);
    w.start(1000);
    CHECK(w.running() && w.elapsed_ms(1000) == 0 && w.elapsed_ms(8250) == 7250);
    w.stop(9000);
    CHECK(!w.running() && w.elapsed_ms(50000) == 8000);  // paused: time stands still
    w.start(60000);
    CHECK(w.elapsed_ms(61500) == 9500);                  // resumes where it was
    w.start(70000);                                      // start while running changes nothing
    CHECK(w.elapsed_ms(61500) == 9500);
    w.stop(61500);
    w.stop(99999);                                       // stop while stopped changes nothing
    CHECK(w.elapsed_ms(99999) == 9500);
    w.reset();
    CHECK(!w.running() && w.elapsed_ms(99999) == 0 && w.laps().empty());
    CHECK(w.elapsed_ms(0) == 0);
    Stopwatch odd;
    odd.start(5000);
    CHECK(odd.elapsed_ms(4000) == 0);  // a clock that reads earlier never underflows
}

TEST(laps_keep_the_stopwatch_running_and_stay_until_reset)
{
    Stopwatch w;
    CHECK(!w.lap(100));  // not running
    w.start(0);
    CHECK(w.lap(12340) && w.lap(30000) && w.lap(31000));
    CHECK(w.running());
    CHECK(w.laps().size() == 3);
    CHECK(w.laps()[0].number == 1 && w.laps()[0].lap_ms == 12340 && w.laps()[0].total_ms == 12340);
    CHECK(w.laps()[1].number == 2 && w.laps()[1].lap_ms == 17660 && w.laps()[1].total_ms == 30000);
    CHECK(w.laps()[2].lap_ms == 1000 && w.laps()[2].total_ms == 31000);
    CHECK(w.laps_text() == "3|1000|31000\n2|17660|30000\n1|12340|12340");  // newest first
    CHECK(w.fastest_lap() == 3 && w.slowest_lap() == 2);
    w.stop(40000);
    CHECK(!w.lap(41000) && w.laps().size() == 3);       // no laps while stopped
    CHECK(w.laps().size() == 3);                         // laps stay when stopped
    w.start(50000);
    CHECK(w.lap(52000) && w.laps().back().lap_ms == 11000 && w.laps().back().total_ms == 42000);  // pause is not counted
    w.reset();
    CHECK(w.laps().empty() && w.laps_text().empty());
    Stopwatch one;
    one.start(0);
    one.lap(1000);
    CHECK(one.fastest_lap() == 0 && one.slowest_lap() == 0);  // needs two laps
    Stopwatch full;
    full.start(0);
    for (size_t i = 0; i < Stopwatch::kMaxLaps; ++i) CHECK(full.lap(1000 * (i + 1)));
    CHECK(!full.lap(999999) && full.laps().size() == 99);
    Stopwatch tie;
    tie.start(0);
    tie.lap(1000);
    tie.lap(2000);
    CHECK(tie.fastest_lap() == 1 && tie.slowest_lap() == 1);  // equal laps: the first one
}

TEST(stopwatch_text)
{
    CHECK(format_stopwatch(0) == "0:00.00" && format_stopwatch(7250) == "0:07.25" && format_stopwatch(59990) == "0:59.99");
    CHECK(format_stopwatch(60000) == "1:00.00" && format_stopwatch(754321) == "12:34.32");
    CHECK(format_stopwatch(3723450) == "1:02:03.45" && format_stopwatch(36000000) == "10:00:00.00");
}

TEST(timers_get_numbered_names)
{
    Timers t;
    CHECK(t.start("", 60, 0) == 1 && t.all()[0].name == "Timer 1");
    CHECK(t.start("", 60, 0) == 2 && t.all()[1].name == "Timer 2");
    CHECK(t.start("Eggs", 60, 0) == 3 && t.all()[2].name == "Eggs");
    CHECK(t.start("Timer", 60, 0) == 4 && t.all()[3].name == "Timer 3");  // the generic name is numbered too
    t.cancel(1);
    CHECK(t.start("", 60, 0) == 5 && t.all().back().name == "Timer 1");   // the lowest free number comes back
    int s;
    std::string name;
    CHECK(parse_timer_request("60", s, name) && name.empty());            // numbering is the timer list's job
    CHECK(parse_timer_request("60|Pasta", s, name) && name == "Pasta");
    Timers r;
    r.restore({{"", 30, 10000}, {"Soup", 30, 10000}, {"Timer", 30, 10000}}, 0, 0);
    CHECK(r.all()[0].name == "Timer 1" && r.all()[1].name == "Soup" && r.all()[2].name == "Timer 2");
}

TEST(weekdays_and_times)
{
    CHECK(weekday_of(at(0, 0, 0)) == 0 && weekday_of(at(6, 23, 59, 59)) == 6 && weekday_of(at(7, 0, 0)) == 0);
    CHECK(weekday_of(0) == 3 && weekday_of(-1) == 2);  // 1970-01-01 Thursday; the day before was a Wednesday
    CHECK(seconds_of_day(at(0, 7, 30, 15)) == 7 * 3600 + 30 * 60 + 15 && seconds_of_day(-1) == 86399);
    CHECK(format_alarm_time(7, 5) == "07:05" && format_alarm_time(23, 59) == "23:59");
    CHECK(format_alarm_time(99, -5) == "23:00" && format_alarm_time(-1, 75) == "00:59");  // clamped, always HH:MM
    int h, m, d;
    std::string label;
    CHECK(parse_alarm_request("7|30|31|Work", h, m, d, label) && h == 7 && m == 30 && d == 31 && label == "Work");
    CHECK(parse_alarm_request("0|0|0|", h, m, d, label) && label.empty());
    CHECK(parse_alarm_request("6|45|127|a|b\nc", h, m, d, label) && label == "a b c" && d == 127);
    for (const char *bad : {"", "7|30", "24|0|0|x", "7|60|0|x", "7|30|128|x", "x|30|0|x", "7||0|x", "7|30|0", "1234|0|0|x"})
        CHECK(!parse_alarm_request(bad, h, m, d, label));
}

TEST(an_alarm_rings_at_its_time_and_a_once_alarm_is_then_spent)
{
    Alarms a;
    const int id = a.add(7, 30, 0, "Wake up", at(0, 7, 0));
    CHECK(id == 1 && a.all()[0].next_fire_s == at(0, 7, 30));
    CHECK(a.tick(at(0, 7, 0)).empty() && !a.ringing());
    CHECK(a.tick(at(0, 7, 29, 59)).empty());
    const std::vector<int> fired = a.tick(at(0, 7, 30));
    CHECK(fired.size() == 1 && fired[0] == id && a.ringing());
    CHECK(a.tick(at(0, 7, 30, 1)).empty());  // fires once
    CHECK(a.next(at(0, 7, 31)) == 0);        // nothing else is coming
    CHECK(a.dismiss() && !a.ringing());
    CHECK(!a.all()[0].enabled);              // a once-alarm switches itself off
    CHECK(a.tick(at(1, 7, 30)).empty());     // and does not ring tomorrow
    CHECK(!a.dismiss());                     // nothing ringing now
}

TEST(setting_an_alarm_for_this_minute_means_tomorrow)
{
    Alarms a;
    a.add(7, 30, 0, "", at(0, 7, 30, 20));
    CHECK(a.all()[0].next_fire_s == at(1, 7, 30));
    Alarms b;
    b.add(7, 30, 0, "", at(0, 7, 30, 0));  // exactly the time: also tomorrow
    CHECK(b.all()[0].next_fire_s == at(1, 7, 30));
    Alarms c;
    c.add(6, 0, 0, "", at(0, 7, 0));       // earlier today: tomorrow
    CHECK(c.all()[0].next_fire_s == at(1, 6, 0));
}

TEST(repeat_days)
{
    Alarms a;
    a.add(6, 45, kDaysWeekdays, "Work", at(4, 12, 0));  // Friday noon
    CHECK(a.all()[0].next_fire_s == at(7, 6, 45));      // next Monday (skips Saturday and Sunday)
    CHECK(a.tick(at(4, 12, 0)).empty());
    CHECK(a.tick(at(7, 6, 45)).size() == 1);
    CHECK(a.dismiss());
    CHECK(a.all()[0].enabled);                          // a repeating alarm stays on
    CHECK(a.all()[0].next_fire_s == at(8, 6, 45));      // Tuesday
    Alarms w;
    w.add(9, 0, kDaysWeekend, "", at(0, 8, 0));         // Monday: the next Saturday
    CHECK(w.all()[0].next_fire_s == at(5, 9, 0));
    Alarms e;
    e.add(9, 0, kDaysAll, "", at(0, 8, 0));
    CHECK(e.all()[0].next_fire_s == at(0, 9, 0));
    Alarms one;
    one.add(9, 0, 1 << 2, "", at(2, 10, 0));            // Wednesday only, set on Wednesday after the time
    CHECK(one.all()[0].next_fire_s == at(9, 9, 0));     // next Wednesday
}

TEST(snooze_rings_again_and_the_tray_shows_it)
{
    Alarms a;
    a.add(7, 30, kDaysAll, "", at(0, 7, 0));
    a.tick(at(0, 7, 30));
    CHECK(a.ringing());
    CHECK(a.snooze(at(0, 7, 30, 5)));       // default 9 minutes
    CHECK(!a.ringing() && a.next_text() == "07:39");
    CHECK(a.tick(at(0, 7, 38, 59)).empty());
    CHECK(a.tick(at(0, 7, 39, 5)).size() == 1 && a.ringing());
    CHECK(a.snooze(at(0, 7, 39, 10), 5));
    CHECK(a.next_text() == "07:44");
    CHECK(a.tick(at(0, 7, 44, 10)).size() == 1);
    CHECK(a.dismiss());
    CHECK(a.next_text() == "07:30" && a.all()[0].next_fire_s == at(1, 7, 30));  // back to its normal time tomorrow
    Alarms once;                             // a once-alarm: still on while snoozed, off after the final dismiss
    once.add(8, 0, 0, "", at(0, 7, 0));
    once.tick(at(0, 8, 0));
    CHECK(once.snooze(at(0, 8, 0)) && once.all()[0].enabled);
    once.tick(at(0, 8, 9));
    CHECK(once.dismiss() && !once.all()[0].enabled && once.next_text().empty());
    CHECK(!once.snooze(at(0, 9, 0)));        // nothing ringing: nothing to snooze
}

TEST(an_untouched_alarm_stops_after_five_minutes)
{
    Alarms a;
    a.add(7, 30, 0, "", at(0, 7, 0));
    a.add(7, 30, kDaysAll, "", at(0, 7, 0));
    CHECK(a.tick(at(0, 7, 30)).size() == 2);
    CHECK(a.tick(at(0, 7, 34, 59)).empty() && a.ringing());
    CHECK(a.tick(at(0, 7, 35)).empty() && !a.ringing());
    CHECK(!a.all()[0].enabled && a.all()[1].enabled);   // the once-alarm is spent, the daily one stays
}

TEST(late_alarms_after_sleep_and_clock_changes)
{
    Alarms a;
    a.add(7, 30, 0, "", at(0, 7, 0));
    a.tick(at(0, 7, 1));
    CHECK(a.tick(at(0, 7, 31, 30)).size() == 1);       // woke 90 s late: rings
    Alarms b;
    b.add(7, 30, kDaysAll, "", at(0, 7, 0));
    b.tick(at(0, 7, 1));
    CHECK(b.tick(at(0, 9, 0)).empty() && !b.ringing());  // woke two hours late: skipped
    CHECK(b.all()[0].next_fire_s == at(1, 7, 30));        // and armed for tomorrow
    Alarms c;
    c.add(7, 30, 0, "", at(0, 7, 0));
    c.tick(at(0, 7, 1));
    CHECK(c.tick(at(0, 10, 0)).empty() && !c.all()[0].enabled);  // a missed once-alarm is spent
    Alarms d;                                          // the clock sets itself (time sync) after the alarm was armed
    d.add(7, 30, kDaysAll, "", 1000);                  // armed against the unset clock (1970)
    d.clock_changed(at(0, 6, 0));
    CHECK(d.all()[0].next_fire_s == at(0, 7, 30) && d.tick(at(0, 6, 0, 1)).empty());
    CHECK(d.tick(at(0, 7, 30)).size() == 1);
    Alarms e;                                          // the clock goes back an hour
    e.add(7, 30, kDaysAll, "", at(0, 7, 0));
    e.tick(at(0, 8, 0));
    e.tick(at(0, 7, 0));
    CHECK(e.all()[0].next_fire_s == at(0, 7, 30));
}

TEST(managing_alarms)
{
    Alarms a;
    CHECK(a.add(24, 0, 0, "", 0) == 0 && a.add(0, 60, 0, "", 0) == 0 && a.add(0, 0, 128, "", 0) == 0 && a.add(-1, 0, 0, "", 0) == 0);
    const int x = a.add(18, 0, 0, "Dinner", at(0, 9, 0));
    const int y = a.add(6, 30, kDaysWeekdays, "Run", at(0, 9, 0));
    CHECK(x == 1 && y == 2);
    CHECK(a.list_text() == "2|06:30|31|1|0|Run\n1|18:00|0|1|0|Dinner");  // by time of day
    CHECK(a.next(at(0, 9, 0)) == 1);                   // today 18:00 comes before tomorrow 06:30
    int64_t until = 0;
    CHECK(a.next(at(0, 9, 0), &until) == 1 && until == 9 * 3600);
    CHECK(a.next_text() == "18:00");
    CHECK(a.set_enabled(x, false, at(0, 9, 0)) && a.next_text() == "06:30" && !a.find(x)->enabled);
    CHECK(a.set_enabled(x, true, at(0, 19, 0)) && a.find(x)->next_fire_s == at(1, 18, 0));  // re-armed from now
    CHECK(a.update(y, 7, 0, kDaysAll, "Walk|\n", at(0, 9, 0)) && a.find(y)->hour == 7 && a.find(y)->label == "Walk  ");
    CHECK(!a.update(99, 7, 0, 0, "", 0) && !a.update(y, 25, 0, 0, "", 0) && !a.set_enabled(99, true, 0) && !a.remove(99));
    CHECK(a.remove(x) && a.all().size() == 1 && a.find(x) == nullptr);
    CHECK(a.add(1, 0, 0, std::string(100, 'x'), 0) != 0 && a.all().back().label.size() == 40);
    Alarms full;
    for (size_t i = 0; i < Alarms::kMax; ++i) CHECK(full.add(1, static_cast<int>(i), 0, "", 0) != 0);
    CHECK(full.add(2, 0, 0, "", 0) == 0);              // the ninth is refused
    Alarms none;
    CHECK(none.next(0) == 0 && none.next_text().empty() && none.list_text().empty() && none.tick(5).empty() && !none.snooze(5));
    CHECK(Alarms().add(7, 30, 0, "", 0, false) == 1);  // added switched off: not armed
    Alarms off;
    off.add(7, 30, 0, "", at(0, 7, 0), false);
    CHECK(off.all()[0].next_fire_s == -1 && off.tick(at(0, 7, 30)).empty() && off.next_text().empty());
}

int main()
{
    RUN(stopwatch_counts_up_and_pauses);
    RUN(laps_keep_the_stopwatch_running_and_stay_until_reset);
    RUN(stopwatch_text);
    RUN(timers_get_numbered_names);
    RUN(weekdays_and_times);
    RUN(an_alarm_rings_at_its_time_and_a_once_alarm_is_then_spent);
    RUN(setting_an_alarm_for_this_minute_means_tomorrow);
    RUN(repeat_days);
    RUN(snooze_rings_again_and_the_tray_shows_it);
    RUN(an_untouched_alarm_stops_after_five_minutes);
    RUN(late_alarms_after_sleep_and_clock_changes);
    RUN(managing_alarms);
    return report("clock tools");
}
