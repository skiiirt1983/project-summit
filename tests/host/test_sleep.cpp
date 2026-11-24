#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/power_service.h"
#include "summit/sleep.h"
#include "summit/system_settings.h"

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
    CHECK(woke.all().size() == 1 && woke.all()[0].name == "Timer 1" && woke.all()[0].id != 0);  // an empty saved name is numbered
}

TEST(wake_plan)
{
    CHECK(wake_plan(WakeCause::Timer).ring_first && !wake_plan(WakeCause::Timer).clock_first);
    CHECK(wake_plan(WakeCause::Motion).clock_first && !wake_plan(WakeCause::Motion).ring_first);
    CHECK(wake_plan(WakeCause::Button).clock_first);
    CHECK(!wake_plan(WakeCause::PowerOn).ring_first && !wake_plan(WakeCause::PowerOn).clock_first);
    CHECK(std::string(wake_cause_name(WakeCause::Motion)) == "motion" && std::string(wake_cause_name(WakeCause::Timer)) == "timer");
}

namespace {

struct FakePort : SleepPort {
    int entered = 0;
    std::vector<uint8_t> blob;
    int64_t wake_in_ms = 0;
    void enter(const std::vector<uint8_t> &snapshot, int64_t wake) override
    {
        ++entered;
        blob = snapshot;
        wake_in_ms = wake;
    }
};

struct Rig {
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    TimerService timers{bus, state};
    FakePort port;
    PowerService power{bus, state, settings, timers, port};
    uint64_t now = 0;
    Rig() { settings.load(); }
    void run(uint64_t ms)  // the UI loop, 4 ticks a second
    {
        const uint64_t end = now + ms;
        while (now < end) {
            now += 250;
            bus.dispatch();
            timers.tick(now);
            power.tick(now);
        }
        bus.dispatch();
    }
    std::string str(const char *k) { return std::get<std::string>(state.get(k)); }
};

}  // namespace

TEST(service_sleeps_when_idle_and_wakes_for_a_timer)
{
    Rig r;
    CHECK(std::get<int64_t>(r.settings.get("power/idle_sleep_min")) == 5 &&
          std::get<bool>(r.settings.get("power/stay_on_charging")));
    CHECK(r.str("power/wake_cause") == "power_on");
    r.run(1000);
    r.bus.publish("timers/start", std::string("14400|Stew: Step 4 of 6"));  // 4 hours
    r.run(4 * kMin);
    CHECK(r.port.entered == 0 && r.str("power/sleep_blocker") == "active");
    r.bus.publish("input/activity", true);  // a touch restarts the idle time
    r.run(4 * kMin + 30000);
    CHECK(r.port.entered == 0);
    r.run(31000);
    CHECK(r.port.entered == 1 && r.power.asleep() && r.str("power/sleep_blocker").empty());
    // Wakes 5 s before the timer; the timer is in the blob.
    SleepSnapshot snap;
    CHECK(decode_snapshot(r.port.blob.data(), r.port.blob.size(), snap) && snap.timers.size() == 1);
    const int64_t left = snap.timers.empty() ? 0 : static_cast<int64_t>(snap.timers[0].remaining_ms);
    CHECK(r.port.wake_in_ms == left - 5000 && left > 3 * 3600 * 1000);
    CHECK(decode_snapshot(r.port.blob.data(), r.port.blob.size(), snap) && snap.timers.size() == 1 &&
          snap.timers[0].name == "Stew: Step 4 of 6" && snap.wake_for == WakeFor::Timer);
    r.run(10 * kMin);
    CHECK(r.port.entered == 1);  // asleep: nothing more happens

    // Deep sleep restarts the firmware: a fresh rig, clock from 0, woken by the RTC timer.
    Rig w;
    std::vector<std::string> critical;
    w.bus.subscribe("notify/toast_critical", [&](const std::string &, const Value &v) { critical.push_back(std::get<std::string>(v)); });
    w.power.on_wake(WakeCause::Timer, r.port.blob.data(), r.port.blob.size(), static_cast<uint64_t>(r.port.wake_in_ms), 0);
    CHECK(w.str("power/wake_cause") == "timer" && w.timers.timers().all().size() == 1);
    w.run(4750);
    CHECK(critical.empty());
    w.run(500);
    CHECK(critical.size() == 1 && critical[0] == "Timer done: Stew: Step 4 of 6");  // on time
    // While it rings the device stays awake; a minute later it stops and the idle time runs again.
    w.run(4 * kMin);
    CHECK(w.port.entered == 0);
    w.run(3 * kMin);
    CHECK(w.port.entered == 1 && w.port.wake_in_ms == -1);
}

TEST(service_blockers_and_settings)
{
    Rig r;
    r.state.set("power/usb_power", true);
    r.run(6 * kMin);
    CHECK(r.port.entered == 0 && r.str("power/sleep_blocker") == "charging");
    r.settings.set("power/stay_on_charging", false);
    r.run(2000);
    CHECK(r.port.entered == 1);

    Rig k;  // recipe open: stays on, but only for 30 minutes without a touch
    k.state.set("ui/keep_awake", true);
    k.run(29 * kMin);
    CHECK(k.port.entered == 0 && k.str("power/sleep_blocker") == "keep_awake");
    k.run(2 * kMin);
    CHECK(k.port.entered == 1);

    Rig u;
    u.state.set("update/in_progress", true);
    u.state.set("sensor/face_down", true);
    u.run(6 * kMin);
    CHECK(u.port.entered == 0 && u.str("power/sleep_blocker") == "updating");

    Rig f;  // face-down: 30 s, even right after a touch
    f.run(1000);
    f.bus.publish("input/activity", true);
    f.state.set("sensor/face_down", true);
    f.run(29000);
    CHECK(f.port.entered == 0);
    f.run(2000);
    CHECK(f.port.entered == 1);

    Rig n;  // idle sleep off; an alarm sets the wake time
    n.settings.set("power/idle_sleep_min", int64_t{0});
    n.state.set("alarm/next_in_s", int64_t{3600});
    n.run(20 * kMin);
    CHECK(n.port.entered == 0 && n.str("power/sleep_blocker") == "disabled");
    n.settings.set("power/idle_sleep_min", int64_t{1});
    n.run(2000);
    SleepSnapshot snap;
    CHECK(n.port.entered == 1 && n.port.wake_in_ms == 3600 * 1000 - 5000 &&
          decode_snapshot(n.port.blob.data(), n.port.blob.size(), snap) && snap.wake_for == WakeFor::Alarm &&
          snap.next_alarm_s == 3600);
}

TEST(service_wake_by_motion_and_bad_blob)
{
    Rig r;
    r.run(1000);
    r.bus.publish("timers/start", std::string("600|Eggs"));
    r.run(6 * kMin);
    CHECK(r.port.entered == 1);
    Rig w;  // picked up after 2 minutes
    w.power.on_wake(WakeCause::Motion, r.port.blob.data(), r.port.blob.size(), 2 * kMin, 0);
    CHECK(w.str("power/wake_cause") == "motion" && w.timers.timers().all().size() == 1);
    const int64_t left = w.timers.timers().next_due_ms(0);
    CHECK(left > 100 * 1000 && left < 4 * static_cast<int64_t>(kMin));
    w.run(4 * kMin);
    CHECK(w.port.entered == 0);  // the wake counts as activity; the timer rang in between
    // Damaged blob, or a cold start: no timers, no crash.
    std::vector<uint8_t> bad = r.port.blob;
    bad[6] ^= 0xFF;
    Rig b;
    b.power.on_wake(WakeCause::Timer, bad.data(), bad.size(), 1000, 0);
    CHECK(b.timers.timers().all().empty() && b.str("power/wake_cause") == "timer");
    Rig c;
    c.power.on_wake(WakeCause::PowerOn, r.port.blob.data(), r.port.blob.size(), 0, 0);
    CHECK(c.timers.timers().all().empty());
    Rig z;
    z.power.on_wake(WakeCause::Button, nullptr, 0, 0, 0);
    CHECK(z.str("power/wake_cause") == "button");
}

TEST(face_down_on_usb_is_dnd_without_deep_sleep)
{
    // Pure rules: face-down on USB, "stay on" off (so charging alone would not block): never sleeps.
    SleepInput in = idle_on_battery();
    in.on_usb_power = true;
    in.stay_on_when_charging = false;
    in.last_activity_ms = in.now_ms;
    in.face_down = true;
    in.face_down_since_ms = in.now_ms - 29999;
    SleepDecision d = decide_sleep(in);
    CHECK(d.blocker == SleepBlocker::FaceDownUsb && !d.dnd && !d.screen_off);
    in.face_down_since_ms = 0;  // long enough: screen off and muted, still awake
    d = decide_sleep(in);
    CHECK(d.blocker == SleepBlocker::FaceDownUsb && d.dnd && d.screen_off && !d.sleep());
    CHECK(std::string(sleep_blocker_name(d.blocker)) == "face_down_usb");
    in.ringing = true;  // alarms and timers still ring, and the screen comes on for them
    d = decide_sleep(in);
    CHECK(d.blocker == SleepBlocker::Ringing && d.dnd && !d.screen_off);
    in.ringing = false;
    in.face_down = false;  // turned back over: normal rules (stay-on off, idle: may sleep)
    in.last_activity_ms = 0;
    d = decide_sleep(in);
    CHECK(d.sleep() && !d.dnd && !d.screen_off);
    // On battery, face-down still sleeps after 30 s and is DND too.
    in = idle_on_battery();
    in.last_activity_ms = in.now_ms;
    in.face_down = true;
    in.face_down_since_ms = 0;
    d = decide_sleep(in);
    CHECK(d.sleep() && d.dnd && d.screen_off);

    Rig r;  // service: state topics follow the rules, and nothing sleeps for 2 hours face-down on USB
    r.state.set("power/usb_power", true);
    r.settings.set("power/stay_on_charging", false);
    r.run(1000);
    r.bus.publish("input/activity", true);
    r.state.set("sensor/face_down", true);
    r.run(29000);
    CHECK(std::get<bool>(r.state.get("power/dnd")) == false && std::get<bool>(r.state.get("power/screen_off")) == false);
    r.run(2000);
    CHECK(std::get<bool>(r.state.get("power/dnd")) && std::get<bool>(r.state.get("power/screen_off")));
    r.run(120 * kMin);
    CHECK(r.port.entered == 0 && r.str("power/sleep_blocker") == "face_down_usb");
    r.state.set("sensor/face_down", false);  // pick it up: screen back on, DND off
    r.run(2000);
    CHECK(std::get<bool>(r.state.get("power/dnd")) == false && std::get<bool>(r.state.get("power/screen_off")) == false);
}

int main()
{
    RUN(idle_and_face_down_enter_sleep);
    RUN(what_blocks_sleep);
    RUN(wakes_for_the_earliest_due_item_minus_the_margin);
    RUN(snapshot_round_trip_and_rejects_damage);
    RUN(timers_survive_sleep);
    RUN(wake_plan);
    RUN(service_sleeps_when_idle_and_wakes_for_a_timer);
    RUN(service_blockers_and_settings);
    RUN(face_down_on_usb_is_dnd_without_deep_sleep);
    RUN(service_wake_by_motion_and_bad_blob);
    return report("sleep scheduler");
}
