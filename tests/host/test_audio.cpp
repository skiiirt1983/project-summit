#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "check.h"
#include "fake_storage.h"
#include "summit/audio.h"
#include "summit/system_settings.h"

using namespace summit;

namespace {

struct FakePort : AudioPort {
    int opens = 0, closes = 0, writes = 0;
    bool is_open = false;
    std::vector<int> volumes;
    size_t samples = 0;
    int peak = 0;  // loudest sample written
    std::function<void()> on_write;
    bool open(int rate) override
    {
        CHECK(rate == kAudioSampleRate);
        ++opens;
        is_open = true;
        return true;
    }
    void set_volume(int p) override { volumes.push_back(p); }
    bool write(const int16_t *d, size_t n) override
    {
        CHECK(is_open);
        for (size_t i = 0; i < n; ++i) peak = std::max(peak, std::abs(static_cast<int>(d[i])));
        ++writes;
        samples += n;
        if (on_write) on_write();
        return true;
    }
    void close() override
    {
        ++closes;
        is_open = false;
    }
};

struct Rig {
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    FakePort port;
    AudioService *svc = nullptr;
    uint64_t now = 1000;
    Rig()
    {
        settings.load();
        svc = new AudioService(bus, state, settings, port);
    }
    ~Rig() { delete svc; }
    void step(uint64_t ms = 20)
    {
        now += ms;
        bus.dispatch();
        svc->tick(now);
    }
};

}  // namespace

TEST(rendering_and_clips)
{
    CHECK(sound_length_ms(Sound::Preview) == 30);
    const std::vector<int16_t> pcm = render_notes(sound_notes(Sound::Preview), 16000, 16000);
    CHECK(pcm.size() == 30u * 16);
    int peak = 0;
    for (int16_t s : pcm) peak = std::max(peak, std::abs(static_cast<int>(s)));
    CHECK(peak > 12000 && peak <= 16000);
    CHECK(pcm.front() == 0 && std::abs(pcm.back()) < 800);  // faded: no click at the ends
    const std::vector<int16_t> quiet = render_notes({{0.f, 10}}, 16000, 16000);  // a rest is silence
    CHECK(quiet.size() == 160);
    for (int16_t s : quiet) CHECK(s == 0);
    CHECK(render_notes(sound_notes(Sound::Preview), 0, 100).empty());
    CHECK(render_notes({{440.f, 10}}, 16000, 99999).size() == 160);  // peak is clamped
    // The recordings: present, the expected length, audible, and decoded to the right loudness.
    CHECK(sound_clip(Sound::Preview) == nullptr);
    for (Sound s : {Sound::Warning, Sound::UsbPlug, Sound::Alarm}) {
        const SoundClip *c = sound_clip(s);
        CHECK(c && c->samples > 8000 && sound_notes(s).empty());
        int loud = 0;
        for (size_t i = 0; c && i < c->samples; ++i) loud = std::max(loud, std::abs(static_cast<int>(ulaw_decode(c->ulaw[i]))));
        CHECK(loud > 8000);
    }
    CHECK(sound_length_ms(Sound::Warning) == 600 && sound_length_ms(Sound::UsbPlug) == 520 &&
          sound_length_ms(Sound::Alarm) == 9750);  // the cowbell loop
    // mu-law decode: silence, sign, and the extremes
    CHECK(ulaw_decode(0xFF) == 0 && ulaw_decode(0x7F) == 0 && ulaw_decode(0x00) == -32124 && ulaw_decode(0x80) == 32124);
    CHECK(ulaw_decode(0x80 + 5) > 0 && ulaw_decode(5) < 0);
}

TEST(rules)
{
    Sound s;
    CHECK(parse_sound("warning", s) && s == Sound::Warning && parse_sound("usb", s) && s == Sound::UsbPlug &&
          parse_sound("alarm", s) && s == Sound::Alarm && parse_sound("preview", s) && s == Sound::Preview &&
          !parse_sound("Warning", s) && !parse_sound("confirm", s) && !parse_sound("chime", s) && !parse_sound("", s));
    CHECK(audible(Sound::UsbPlug, false, 50) && !audible(Sound::UsbPlug, true, 50) && !audible(Sound::UsbPlug, false, 0));
    CHECK(audible(Sound::Warning, true, 50) && !audible(Sound::Warning, false, 0));
    CHECK(audible(Sound::Preview, true, 50) && !audible(Sound::Preview, false, 0));
    CHECK(audible(Sound::Alarm, true, 0));
    CHECK(effective_volume(Sound::Alarm, 0) == kAlarmMinVolume && effective_volume(Sound::Alarm, 80) == 80);
    CHECK(effective_volume(Sound::Warning, 0) == 0 && effective_volume(Sound::Warning, 250) == 100 &&
          effective_volume(Sound::Warning, -5) == 0);
}

TEST(warning_plays_at_the_saved_volume_then_the_amplifier_goes_off)
{
    Rig r;
    r.step();
    CHECK(r.port.opens == 0);  // nothing asked: stays off
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.samples == 600u * 16);
    CHECK(r.port.volumes.size() == 1 && r.port.volumes[0] == codec_volume(60));  // the default setting
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.samples == 1200u * 16 && r.port.volumes.size() == 1);  // still open, volume unchanged
    r.step(600 + AudioService::kIdleCloseMs - 100);  // the idle time counts from the end of the sound
    CHECK(r.port.is_open);
    r.step(300);
    CHECK(!r.port.is_open && r.port.closes == 1);
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    CHECK(r.port.opens == 2 && r.port.volumes.size() == 2);  // reopened: the volume is set again
}

TEST(volume_curve_and_loudness)
{
    CHECK(codec_volume(0) == 0 && codec_volume(-4) == 0);
    CHECK(codec_volume(1) == kCodecMinVolume && codec_volume(100) == 100 && codec_volume(250) == 100);
    for (int v = 1; v < 100; ++v) CHECK(codec_volume(v + 1) >= codec_volume(v));  // never gets quieter as it goes up
    CHECK(codec_volume(50) > codec_volume(25) && codec_volume(25) > kCodecMinVolume);
    for (Sound s : {Sound::Preview, Sound::Warning, Sound::UsbPlug, Sound::Alarm}) {  // every sound reaches the same peak
        Rig r;
        r.bus.publish("audio/test", std::string(s == Sound::Preview ? "preview" : s == Sound::Warning ? "warning" : s == Sound::UsbPlug ? "usb" : "alarm"));
        r.step();
        CHECK(r.port.peak >= kTargetPeak * 3 / 4 && r.port.peak <= kTargetPeak);  // the 3 s test alarm may miss the loudest hit
        CHECK(sound_gain_q8(s) >= 256);
    }
}

TEST(bad_requests_and_silence)
{
    Rig r;
    r.bus.publish("audio/play", std::string("klaxon"));
    r.bus.publish("audio/play", int64_t{3});
    r.step();
    CHECK(r.port.opens == 0);
    r.settings.set("audio/volume", int64_t{0});
    r.step();  // saving the volume at 0 plays nothing
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    CHECK(r.port.opens == 0 && r.port.writes == 0);
}

TEST(do_not_disturb_mutes_the_usb_sound_not_warning_or_alarm)
{
    Rig r;
    r.settings.set("system/dnd", true);
    r.step();
    r.bus.publish("audio/play", std::string("usb"));
    r.step();
    CHECK(r.port.samples == 0);
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    const size_t warn = r.port.samples;
    CHECK(warn == 600u * 16);
    r.bus.publish("audio/play", std::string("alarm"));
    r.step();
    CHECK(r.port.samples > warn);
    r.settings.set("system/dnd", false);
    r.state.set("power/dnd", true);  // face-down mute counts too
    r.step();
    const size_t before = r.port.samples;
    r.bus.publish("audio/play", std::string("usb"));
    r.step();
    CHECK(r.port.samples == before);
    r.state.set("power/dnd", false);
    r.bus.publish("audio/play", std::string("usb"));
    r.step();
    CHECK(r.port.samples == before + 520u * 16);
}

TEST(diagnostics_test_buttons_play_the_whole_sound_even_in_do_not_disturb)
{
    Rig r;
    r.settings.set("system/dnd", true);
    r.step();
    r.bus.publish("audio/test", std::string("usb"));
    r.step();
    CHECK(r.port.samples == 520u * 16);  // the real USB sound would be muted; the button asks for it
    const size_t before = r.port.samples;
    r.bus.publish("audio/test", std::string("alarm"));  // nothing is ringing
    r.step();
    CHECK(r.port.samples == before + AudioService::kTestAlarmMs * 16);  // 3 s of the ring sound, not one 64 ms piece
    r.settings.set("audio/volume", int64_t{0});
    r.step();
    r.bus.publish("audio/test", std::string("usb"));
    r.step();
    CHECK(r.port.samples == before + AudioService::kTestAlarmMs * 16);  // volume 0 still silences it
    r.bus.publish("audio/play", std::string("alarm"));  // a real alarm request with nothing ringing stays a stub
    r.step();
    CHECK(r.port.samples <= before + AudioService::kTestAlarmMs * 16 + 1024);
}

TEST(only_critical_toasts_and_usb_plug_ins_make_a_sound)
{
    Rig r;
    r.bus.publish("notify/toast", std::string("Module installed"));  // an ordinary toast: silent
    r.step();
    CHECK(r.port.samples == 0);
    r.bus.publish("notify/toast_critical", std::string("Battery low"));
    r.step();
    CHECK(r.port.samples == 600u * 16);
    // USB: the start-up value is not a plug-in; unplugging is silent; plugging in plays the sound
    r.state.set("power/usb_power", true);  // the first reading after start-up is where things stand, not a plug-in
    r.step();
    CHECK(r.port.samples == 600u * 16);
    r.state.set("power/usb_power", false);
    r.step();
    r.state.set("power/usb_power", true);
    r.step();
    CHECK(r.port.samples == 600u * 16 + 520u * 16);
}

TEST(usb_edges)
{
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    settings.load();
    state.set("power/usb_power", true);  // already on when the service starts: not a plug-in
    FakePort port;
    AudioService svc(bus, state, settings, port);
    uint64_t now = 1000;
    auto step = [&] {
        now += 20;
        bus.dispatch();
        svc.tick(now);
    };
    state.set("power/usb_power", false);
    step();
    CHECK(port.samples == 0);
    state.set("power/usb_power", true);
    step();
    CHECK(port.samples == 520u * 16);
    state.set("power/usb_power", true);  // unchanged: nothing
    step();
    CHECK(port.samples == 520u * 16);
}

TEST(a_timer_done_toast_does_not_play_over_the_alarm)
{
    Rig r;
    r.state.set("timers/ringing", true);
    r.bus.publish("notify/toast_critical", std::string("Timer done: Eggs"));  // the timer service sends both
    r.step();
    CHECK(r.port.samples == find_library("kitchen")->clip.samples);  // the ring sound only, no warning hit before it
}

TEST(volume_slider)
{
    Rig r;
    r.bus.publish("ui/volume_live", int64_t{30});  // dragging: the volume follows, no sound by itself
    r.step();
    CHECK(r.port.opens == 0);
    r.settings.set("audio/volume", int64_t{30});  // released: a short tick at the new volume
    r.step();
    CHECK(r.port.opens == 1 && r.port.writes == 1 && r.port.samples == 30u * 16 && r.port.volumes.back() == codec_volume(30));
    r.settings.set("system/dnd", true);
    r.settings.set("audio/volume", int64_t{70});
    r.step();
    CHECK(r.port.writes == 2 && r.port.volumes.back() == codec_volume(70));  // the preview ignores do-not-disturb
}

TEST(timer_alarm_repeats_until_dismissed_and_is_never_silent)
{
    Rig r;
    r.settings.set("audio/volume", int64_t{0});
    r.settings.set("system/dnd", true);
    r.step();
    r.state.set("timers/ringing", true);
    r.step();
    const size_t one = find_library("kitchen")->clip.samples;  // the default timer sound, 5.02 s
    CHECK(r.port.samples == one && r.port.volumes.back() == codec_volume(kAlarmMinVolume));
    r.step(4000);  // still inside the loop
    CHECK(r.port.samples == one);
    r.step(1100);
    CHECK(r.port.samples == 2 * one);  // repeats at once: the loop carries its own gap
    r.state.set("timers/ringing", false);
    r.step();
    r.step(12000);
    CHECK(r.port.samples == 2 * one);  // stopped
    r.step(AudioService::kIdleCloseMs);
    CHECK(!r.port.is_open);
    r.state.set("timers/ringing", true);  // rings again at once the next time
    r.step();
    CHECK(r.port.samples == 3 * one);
}

TEST(the_library_and_the_timer_sound_setting)
{
    size_t n = 0;
    const LibrarySound *all = sound_library(&n);
    CHECK(n == 11);
    int alarms = 0, timers = 0;
    for (size_t i = 0; i < n; ++i) {
        (all[i].alarm ? alarms : timers)++;
        const int ms = static_cast<int>(all[i].clip.samples * 1000 / kAudioSampleRate);
        CHECK(ms >= 700 && ms <= 10000);          // Paul's loops: 0.72 s to 9.75 s
        CHECK(find_library(all[i].id) == &all[i]);
        CHECK(clip_gain_q8(&all[i].clip) >= 256);
    }
    CHECK(alarms == 6 && timers == 5 && !find_library("klaxon") && !find_library(""));
    // The settings offer exactly the library ids, split the same way.
    for (const SettingDef &d : system_settings_schema().defs) {
        if (d.key != "audio/timer_sound" && d.key != "audio/alarm_sound") continue;
        const bool alarm = d.key == "audio/alarm_sound";
        size_t count = 0;
        for (size_t i = 0; i < n; ++i) count += all[i].alarm == alarm;
        CHECK(d.choices.size() == count);
        for (const std::string &c : d.choices) CHECK(find_library(c) && find_library(c)->alarm == alarm);
        CHECK(find_library(std::get<std::string>(d.default_value)) != nullptr);
    }
    CHECK(std::string(kDefaultTimerSound) == "kitchen" && std::string(kDefaultAlarmSound) == "summit-cowbell");
    CHECK(sound_clip(Sound::Alarm) == &find_library(kDefaultAlarmSound)->clip);

    Rig r;  // changing the setting changes what rings
    r.settings.set("audio/timer_sound", std::string("marimba"));
    r.step();
    r.state.set("timers/ringing", true);
    r.step();
    CHECK(r.port.samples == find_library("marimba")->clip.samples);
}

TEST(the_ring_sound_is_read_from_the_setting_at_start_up)
{
    EventBus bus{64};
    StateStore state{bus};
    FakeStorage storage;
    Settings settings{system_settings_schema(), storage, &bus};
    settings.load();
    settings.set("audio/timer_sound", std::string("chisper"));
    FakePort port;
    AudioService svc(bus, state, settings, port);
    state.set("timers/ringing", true);
    bus.dispatch();
    svc.tick(1000);
    CHECK(port.samples == find_library("chisper")->clip.samples);
}

TEST(diagnostics_can_play_any_library_sound_whole)
{
    Rig r;
    r.settings.set("system/dnd", true);
    r.step();
    r.bus.publish("audio/test", std::string("summit-cowbell"));
    r.step();
    CHECK(r.port.samples == find_library("summit-cowbell")->clip.samples);  // 9.75 s, not the 3 s cap
    const size_t before = r.port.samples;
    r.bus.publish("audio/test", std::string("klaxon"));  // unknown ids do nothing
    r.step();
    CHECK(r.port.samples == before);
    r.bus.publish("audio/play", std::string("hi-low"));  // only the test event takes library ids
    r.step();
    CHECK(r.port.samples == before);
}

TEST(dismissing_stops_the_recording_at_once)
{
    Rig r;
    int writes_seen = 0;
    r.port.on_write = [&] {
        if (++writes_seen == 3) {  // someone taps "stop" while the cowbell plays
            r.state.set("timers/ringing", false);
            r.bus.dispatch();
        }
    };
    r.state.set("timers/ringing", true);
    r.step();
    CHECK(writes_seen == 3 && r.port.samples == 3u * 1024);  // not the whole 9 seconds
    r.step();
    r.step(12000);
    CHECK(r.port.samples == 3u * 1024);
    r.step(AudioService::kIdleCloseMs);
    CHECK(!r.port.is_open);
}

int main()
{
    RUN(rendering_and_clips);
    RUN(rules);
    RUN(warning_plays_at_the_saved_volume_then_the_amplifier_goes_off);
    RUN(volume_curve_and_loudness);
    RUN(bad_requests_and_silence);
    RUN(do_not_disturb_mutes_the_usb_sound_not_warning_or_alarm);
    RUN(diagnostics_test_buttons_play_the_whole_sound_even_in_do_not_disturb);
    RUN(only_critical_toasts_and_usb_plug_ins_make_a_sound);
    RUN(usb_edges);
    RUN(a_timer_done_toast_does_not_play_over_the_alarm);
    RUN(volume_slider);
    RUN(timer_alarm_repeats_until_dismissed_and_is_never_silent);
    RUN(the_library_and_the_timer_sound_setting);
    RUN(the_ring_sound_is_read_from_the_setting_at_start_up);
    RUN(diagnostics_can_play_any_library_sound_whole);
    RUN(dismissing_stops_the_recording_at_once);
    return report("audio");
}
