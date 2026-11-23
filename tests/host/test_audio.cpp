#include <cstdlib>
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

TEST(rendering)
{
    CHECK(sound_length_ms(Sound::Preview) == 30 && sound_length_ms(Sound::Chime) == 350 && sound_length_ms(Sound::Alarm) == 1010);
    const std::vector<int16_t> pcm = render_notes(sound_notes(Sound::Chime), 16000, 16000);
    CHECK(pcm.size() == 350u * 16);
    int peak = 0;
    for (int16_t s : pcm) peak = std::max(peak, std::abs(static_cast<int>(s)));
    CHECK(peak > 12000 && peak <= 16000);
    CHECK(pcm.front() == 0 && std::abs(pcm.back()) < 800);  // faded: no click at the ends
    const std::vector<int16_t> quiet = render_notes({{0.f, 10}}, 16000, 16000);  // a rest is silence
    CHECK(quiet.size() == 160);
    for (int16_t s : quiet) CHECK(s == 0);
    CHECK(render_notes(sound_notes(Sound::Alarm), 0, 100).empty());
    CHECK(render_notes({{440.f, 10}}, 16000, 99999).size() == 160);  // peak is clamped
}

TEST(rules)
{
    Sound s;
    CHECK(parse_sound("chime", s) && s == Sound::Chime && parse_sound("alarm", s) && s == Sound::Alarm &&
          parse_sound("preview", s) && s == Sound::Preview && !parse_sound("Chime", s) && !parse_sound("", s));
    CHECK(audible(Sound::Chime, false, 50) && !audible(Sound::Chime, true, 50) && !audible(Sound::Chime, false, 0));
    CHECK(audible(Sound::Preview, true, 50) && !audible(Sound::Preview, false, 0));
    CHECK(audible(Sound::Alarm, true, 0));
    CHECK(effective_volume(Sound::Alarm, 0) == kAlarmMinVolume && effective_volume(Sound::Alarm, 80) == 80);
    CHECK(effective_volume(Sound::Chime, 0) == 0 && effective_volume(Sound::Chime, 250) == 100 &&
          effective_volume(Sound::Chime, -5) == 0);
}

TEST(chime_plays_at_the_saved_volume_then_the_amplifier_goes_off)
{
    Rig r;
    r.step();
    CHECK(r.port.opens == 0);  // nothing asked: stays off
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.writes == 1 && r.port.samples == 350u * 16);
    CHECK(r.port.volumes.size() == 1 && r.port.volumes[0] == 60);  // the default setting
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.opens == 1 && r.port.writes == 2 && r.port.volumes.size() == 1);  // still open, volume unchanged
    r.step(AudioService::kIdleCloseMs - 100);
    CHECK(r.port.is_open);
    r.step(300);
    CHECK(!r.port.is_open && r.port.closes == 1);
    r.bus.publish("audio/play", std::string("chime"));
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
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.opens == 0 && r.port.writes == 0);
}

TEST(do_not_disturb_mutes_chimes_not_alarms)
{
    Rig r;
    r.settings.set("system/dnd", true);
    r.step();
    r.port.writes = 0;
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.writes == 0);
    r.bus.publish("audio/play", std::string("alarm"));
    r.step();
    CHECK(r.port.writes == 1);
    r.settings.set("system/dnd", false);
    r.state.set("power/dnd", true);  // face-down mute counts too
    r.step();
    const int before = r.port.writes;
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.writes == before);
    r.state.set("power/dnd", false);
    r.bus.publish("audio/play", std::string("chime"));
    r.step();
    CHECK(r.port.writes == before + 1);
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
    CHECK(r.port.writes == 1 && r.port.volumes.back() == kAlarmMinVolume);
    r.step(1000);  // still inside the beeps and the gap
    CHECK(r.port.writes == 1);
    r.step(600);
    CHECK(r.port.writes == 2);
    r.step(2000);
    CHECK(r.port.writes == 3);
    r.state.set("timers/ringing", false);
    r.step();
    r.step(3000);
    CHECK(r.port.writes == 3);  // stopped
    r.step(AudioService::kIdleCloseMs);
    CHECK(!r.port.is_open);
    r.state.set("timers/ringing", true);  // rings again at once the next time
    r.step();
    CHECK(r.port.writes == 4);
}

int main()
{
    RUN(rendering);
    RUN(rules);
    RUN(chime_plays_at_the_saved_volume_then_the_amplifier_goes_off);
    RUN(bad_requests_and_silence);
    RUN(do_not_disturb_mutes_chimes_not_alarms);
    RUN(volume_slider);
    RUN(timer_alarm_repeats_until_dismissed_and_is_never_silent);
    return report("audio");
}
