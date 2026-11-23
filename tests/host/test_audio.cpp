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
    std::function<void()> on_write;
    bool open(int rate) override
    {
        CHECK(rate == kAudioSampleRate);
        ++opens;
        is_open = true;
        return true;
    }
    void set_volume(int p) override { volumes.push_back(p); }
    bool write(const int16_t *, size_t n) override
    {
        CHECK(is_open);
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
    for (Sound s : {Sound::Confirm, Sound::Warning, Sound::Alarm}) {
        const SoundClip *c = sound_clip(s);
        CHECK(c && c->samples > 8000 && sound_notes(s).empty());
        int loud = 0;
        for (size_t i = 0; c && i < c->samples; ++i) loud = std::max(loud, std::abs(static_cast<int>(ulaw_decode(c->ulaw[i]))));
        CHECK(loud > 8000);
    }
    CHECK(sound_length_ms(Sound::Confirm) == 800 && sound_length_ms(Sound::Warning) == 1150 &&
          sound_length_ms(Sound::Alarm) > 9000 && sound_length_ms(Sound::Alarm) < 9400);
    // mu-law decode: silence, sign, and the extremes
    CHECK(ulaw_decode(0xFF) == 0 && ulaw_decode(0x7F) == 0 && ulaw_decode(0x00) == -32124 && ulaw_decode(0x80) == 32124);
    CHECK(ulaw_decode(0x80 + 5) > 0 && ulaw_decode(5) < 0);
}

TEST(rules)
{
    Sound s;
    CHECK(parse_sound("confirm", s) && s == Sound::Confirm && parse_sound("warning", s) && s == Sound::Warning &&
          parse_sound("alarm", s) && s == Sound::Alarm && parse_sound("preview", s) && s == Sound::Preview &&
          !parse_sound("Confirm", s) && !parse_sound("chime", s) && !parse_sound("", s));
    CHECK(audible(Sound::Confirm, false, 50) && !audible(Sound::Confirm, true, 50) && !audible(Sound::Confirm, false, 0));
    CHECK(audible(Sound::Warning, true, 50) && !audible(Sound::Warning, false, 0));
    CHECK(audible(Sound::Preview, true, 50) && !audible(Sound::Preview, false, 0));
    CHECK(audible(Sound::Alarm, true, 0));
    CHECK(effective_volume(Sound::Alarm, 0) == kAlarmMinVolume && effective_volume(Sound::Alarm, 80) == 80);
    CHECK(effective_volume(Sound::Confirm, 0) == 0 && effective_volume(Sound::Confirm, 250) == 100 &&
          effective_volume(Sound::Confirm, -5) == 0);
}

TEST(confirm_plays_at_the_saved_volume_then_the_amplifier_goes_off)
{
    Rig r;
    r.step();
    CHECK(r.port.opens == 0);  // nothing asked: stays off
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.samples == 800u * 16);
    CHECK(r.port.volumes.size() == 1 && r.port.volumes[0] == 60);  // the default setting
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.samples == 1600u * 16 && r.port.volumes.size() == 1);  // still open, volume unchanged
    r.step(800 + AudioService::kIdleCloseMs - 100);  // the idle time counts from the end of the sound
    CHECK(r.port.is_open);
    r.step(300);
    CHECK(!r.port.is_open && r.port.closes == 1);
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.opens == 2 && r.port.volumes.size() == 2);  // reopened: the volume is set again
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
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.opens == 0 && r.port.writes == 0);
}

TEST(do_not_disturb_mutes_confirm_not_warning_or_alarm)
{
    Rig r;
    r.settings.set("system/dnd", true);
    r.step();
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.samples == 0);
    r.bus.publish("audio/play", std::string("warning"));
    r.step();
    const size_t warn = r.port.samples;
    CHECK(warn == 1150u * 16);
    r.bus.publish("audio/play", std::string("alarm"));
    r.step();
    CHECK(r.port.samples > warn);
    r.settings.set("system/dnd", false);
    r.state.set("power/dnd", true);  // face-down mute counts too
    r.step();
    const size_t before = r.port.samples;
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.samples == before);
    r.state.set("power/dnd", false);
    r.bus.publish("audio/play", std::string("confirm"));
    r.step();
    CHECK(r.port.samples == before + 800u * 16);
}

TEST(volume_slider)
{
    Rig r;
    r.bus.publish("ui/volume_live", int64_t{30});  // dragging: the volume follows, no sound by itself
    r.step();
    CHECK(r.port.opens == 0);
    r.settings.set("audio/volume", int64_t{30});  // released: a short tick at the new volume
    r.step();
    CHECK(r.port.opens == 1 && r.port.writes == 1 && r.port.samples == 30u * 16 && r.port.volumes.back() == 30);
    r.settings.set("system/dnd", true);
    r.settings.set("audio/volume", int64_t{70});
    r.step();
    CHECK(r.port.writes == 2 && r.port.volumes.back() == 70);  // the preview ignores do-not-disturb
}

TEST(timer_alarm_repeats_until_dismissed_and_is_never_silent)
{
    Rig r;
    r.settings.set("audio/volume", int64_t{0});
    r.settings.set("system/dnd", true);
    r.step();
    r.state.set("timers/ringing", true);
    r.step();
    const size_t one = sound_clip(Sound::Alarm)->samples;
    CHECK(r.port.samples == one && r.port.volumes.back() == kAlarmMinVolume);
    r.step(8000);  // still inside the recording and the gap
    CHECK(r.port.samples == one);
    r.step(2000);
    CHECK(r.port.samples == 2 * one);  // rings again
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
    RUN(confirm_plays_at_the_saved_volume_then_the_amplifier_goes_off);
    RUN(bad_requests_and_silence);
    RUN(do_not_disturb_mutes_confirm_not_warning_or_alarm);
    RUN(volume_slider);
    RUN(timer_alarm_repeats_until_dismissed_and_is_never_silent);
    RUN(dismissing_stops_the_recording_at_once);
    return report("audio");
}
