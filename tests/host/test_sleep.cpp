#include <string>
#include <vector>

#include "check.h"
#include "summit/sleep.h"

using namespace summit;

namespace {
const uint64_t kMin = 60 * 1000;

SleepInput idle_on_battery()
{
    SleepInput in;
    in.now_ms = 10 * kMin;
    in.last_activity_ms = 5 * kMin;  // idle for exactly the 5 min default
    return in;
}
}  // namespace

TEST(idle_and_face_down_enter_sleep)
{
    SleepInput in = idle_on_battery();
    CHECK(decide_sleep(in).sleep() && decide_sleep(in).wake_in_ms == -1 && decide_sleep(in).wake_for == WakeFor::Nothing);
    in.last_activity_ms += 1;  // one millisecond short
    CHECK(decide_sleep(in).blocker == SleepBlocker::Active);
    // Face-down for 30 s sleeps even though it was touched a moment ago.
    in.last_activity_ms = in.now_ms;
    in.face_down = true;
    in.face_down_since_ms = in.now_ms - 29999;
    CHECK(decide_sleep(in).blocker == SleepBlocker::Active);
    in.face_down_since_ms = in.now_ms - 30000;
    CHECK(decide_sleep(in).sleep());
    // The idle timeout is a setting; 0 = never when idle, face-down still sleeps.
    in = idle_on_battery();
    in.idle_sleep_min = 10;
    CHECK(!decide_sleep(in).sleep());
    in.last_activity_ms = 0;
    CHECK(decide_sleep(in).sleep());
    in.idle_sleep_min = 0;
    CHECK(decide_sleep(in).blocker == SleepBlocker::Disabled);
    in.face_down = true;
    in.face_down_since_ms = 0;
    CHECK(decide_sleep(in).sleep());
}

TEST(what_blocks_sleep)
{
    SleepInput in = idle_on_battery();
    in.keep_awake = true;  // recipe open with the screen-on rule
    CHECK(decide_sleep(in).blocker == SleepBlocker::KeepAwake);
    in.keep_awake = false;
    in.updating = true;
    CHECK(decide_sleep(in).blocker == SleepBlocker::Updating);
    in.updating = false;
    in.ringing = true;
    CHECK(decide_sleep(in).blocker == SleepBlocker::Ringing);
    in.ringing = false;
    // USB power: stays on by default; sleeps if the user turned that off.
    in.on_usb_power = true;
    CHECK(decide_sleep(in).blocker == SleepBlocker::Charging);
    in.stay_on_when_charging = false;
    CHECK(decide_sleep(in).sleep());
    // Blockers hold face-down too.
    in = idle_on_battery();
    in.face_down = true;
    in.face_down_since_ms = 0;
    in.updating = true;
    CHECK(!decide_sleep(in).sleep());
    CHECK(std::string(sleep_blocker_name(SleepBlocker::KeepAwake)) == "keep_awake" &&
          std::string(sleep_blocker_name(SleepBlocker::None)).empty());
}

TEST(wakes_for_the_earliest_due_item_minus_the_margin)
{
    SleepInput in = idle_on_battery();
    in.next_timer_ms = 4 * 60 * kMin;  // the 4-hour recipe timer
    SleepDecision d = decide_sleep(in);
    CHECK(d.sleep() && d.wake_in_ms == 4 * 60 * static_cast<int64_t>(kMin) - 5000 && d.wake_for == WakeFor::Timer);
    in.next_alarm_s = 3600;  // an alarm in one hour comes first
    d = decide_sleep(in);
    CHECK(d.sleep() && d.wake_in_ms == 3600 * 1000 - 5000 && d.wake_for == WakeFor::Alarm);
    in.next_timer_ms = 20 * 1000;
    d = decide_sleep(in);
    CHECK(d.sleep() && d.wake_in_ms == 15000 && d.wake_for == WakeFor::Timer);
    in.wake_margin_ms = 8000;  // margin measured on the board
    CHECK(decide_sleep(in).wake_in_ms == 12000);
    // Due within the margin: stay awake for it.
    in.next_timer_ms = 8000;
    CHECK(decide_sleep(in).blocker == SleepBlocker::DueSoon);
    in.next_timer_ms = 0;
    CHECK(decide_sleep(in).blocker == SleepBlocker::DueSoon);
    in.next_timer_ms = -1;
    in.next_alarm_s = 5;
    CHECK(decide_sleep(in).blocker == SleepBlocker::DueSoon);
    in.next_alarm_s = -1;
    CHECK(decide_sleep(in).sleep() && decide_sleep(in).wake_in_ms == -1);
}

TEST(snapshot_round_trip_and_rejects_damage)
{
    SleepSnapshot s;
    s.timers = {{"Pancakes: Step 2 of 3", 1800, 1234567}, {"Eggs", 180, 1}, {std::string(120, 'x'), 60, 60000}};
    s.next_alarm_s = 7 * 3600;
    s.wake_for = WakeFor::Timer;
    std::vector<uint8_t> blob = encode_snapshot(s);
    CHECK(blob.size() <= kSnapshotMaxBytes);
    SleepSnapshot out;
    CHECK(decode_snapshot(blob.data(), blob.size(), out));
    CHECK(out.timers.size() == 3 && out.timers[0].name == "Pancakes: Step 2 of 3" && out.timers[0].total_s == 1800 &&
          out.timers[0].remaining_ms == 1234567 && out.timers[1].remaining_ms == 1);
    CHECK(out.timers[2].name.size() == 80);  // long names are cut
    CHECK(out.next_alarm_s == 7 * 3600 && out.wake_for == WakeFor::Timer);
    // Eight timers with the longest names still fit RTC memory.
    SleepSnapshot full;
    for (int i = 0; i < 12; ++i) full.timers.push_back({std::string(80, 'n'), 100, 1000});
    blob = encode_snapshot(full);
    CHECK(blob.size() <= kSnapshotMaxBytes && decode_snapshot(blob.data(), blob.size(), out) && out.timers.size() == 8);
    // Empty: no alarm, no timers.
    blob = encode_snapshot(SleepSnapshot());
    CHECK(decode_snapshot(blob.data(), blob.size(), out) && out.timers.empty() && out.next_alarm_s == -1);
    // Any flipped bit, a cut blob, garbage or nothing is ignored (out keeps its value).
    blob = encode_snapshot(s);
    out = SleepSnapshot();
    for (size_t i = 0; i < blob.size(); ++i) {
        std::vector<uint8_t> bad = blob;
        bad[i] ^= 0x10;
        CHECK(!decode_snapshot(bad.data(), bad.size(), out));
    }
    CHECK(!decode_snapshot(blob.data(), blob.size() - 1, out) && !decode_snapshot(blob.data(), 3, out));
    CHECK(!decode_snapshot(nullptr, 0, out));
    std::vector<uint8_t> zeros(64, 0);
    CHECK(!decode_snapshot(zeros.data(), zeros.size(), out) && out.timers.empty());
}

TEST(timers_survive_sleep)
{
    Timers t;
    t.start("Stew", 4 * 3600, 0);
    t.start("Eggs", 180, 0);
    const int done = t.start("Done", 1, 0);
    t.tick(2000);  // "Done" is ringing: not saved (ringing blocks sleep anyway)
    CHECK(done != 0 && t.ringing() && t.next_due_ms(2000) == 178000);
    std::vector<SavedTimer> saved = t.save(2000);
    CHECK(saved.size() == 2 && saved[0].name == "Stew" && saved[0].remaining_ms == 4 * 3600 * 1000 - 2000);

    // Through the blob, as on the device; the clock restarts from 0 after deep sleep.
    SleepSnapshot snap;
    snap.timers = saved;
    std::vector<uint8_t> blob = encode_snapshot(snap);
    SleepSnapshot back;
    CHECK(decode_snapshot(blob.data(), blob.size(), back));
    Timers woke;
    const uint64_t slept = 4 * 3600 * 1000 - 2000 - 5000;  // woke 5 s before "Stew"
    woke.restore(back.timers, slept, 700);
    CHECK(woke.all().size() == 2 && woke.next_due_ms(700) == 0);  // "Eggs" ended while asleep
    std::vector<KitchenTimer> fired = woke.tick(700);
    CHECK(fired.size() == 1 && fired[0].name == "Eggs");
    CHECK(woke.next_due_ms(700) == 5000);
    CHECK(woke.tick(5699).empty());
    fired = woke.tick(5700);
    CHECK(fired.size() == 1 && fired[0].name == "Stew" && fired[0].total_s == 4 * 3600);  // on time
    CHECK(Timers().next_due_ms(0) == -1 && Timers().save(0).empty());
    // Restore replaces what was there and ignores nonsense.
    woke.restore({{"", 60, 1000}, {"bad", 0, 1000}}, 0, 0);
    CHECK(woke.all().size() == 1 && woke.all()[0].name == "Timer" && woke.all()[0].id != 0);
}

TEST(wake_plan)
{
    CHECK(wake_plan(WakeCause::Timer).ring_first && !wake_plan(WakeCause::Timer).clock_first);
    CHECK(wake_plan(WakeCause::Motion).clock_first && !wake_plan(WakeCause::Motion).ring_first);
    CHECK(wake_plan(WakeCause::Button).clock_first);
    CHECK(!wake_plan(WakeCause::PowerOn).ring_first && !wake_plan(WakeCause::PowerOn).clock_first);
    CHECK(std::string(wake_cause_name(WakeCause::Motion)) == "motion" && std::string(wake_cause_name(WakeCause::Timer)) == "timer");
}

int main()
{
    RUN(idle_and_face_down_enter_sleep);
    RUN(what_blocks_sleep);
    RUN(wakes_for_the_earliest_due_item_minus_the_margin);
    RUN(snapshot_round_trip_and_rejects_damage);
    RUN(timers_survive_sleep);
    RUN(wake_plan);
    return report("sleep scheduler");
}
