// Audio service: which sound plays when (pure rules, host-tested in tests/host/test_audio.cpp) and the service that
// turns bus events into sounds through an AudioPort (the ES8311 codec + amplifier on the board; a fake in tests).
//
// Sounds (generated, no files):  Preview = a short tick when the volume slider is released,
// Chime = a two-note notification sound, Alarm = three beeps, repeated while a timer rings.
// Rules: volume 0 silences Preview and Chime; do-not-disturb (setting system/dnd or state power/dnd) silences Chime
// only; an Alarm always rings and never quieter than kAlarmMinVolume, so a timer is never missed.
#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/state_store.h"

namespace summit {

enum class Sound { Preview, Chime, Alarm };

struct Note {
    float hz;  // 0 = silence
    int ms;
};

constexpr int kAudioSampleRate = 16000;  // mono, 16-bit
constexpr int kAlarmMinVolume = 40;      // percent

std::vector<Note> sound_notes(Sound s);
int sound_length_ms(Sound s);
// Sine notes with a 5 ms fade in/out so nothing clicks. peak = largest sample (0..32767).
std::vector<int16_t> render_notes(const std::vector<Note> &notes, int sample_rate, int peak);
// Parses the payload of the bus event audio/play: "preview", "chime" or "alarm".
bool parse_sound(const std::string &name, Sound &out);
bool audible(Sound s, bool dnd, int volume);
// Volume handed to the codec (0..100).
int effective_volume(Sound s, int volume);

class AudioPort {
public:
    virtual ~AudioPort() = default;
    virtual bool open(int sample_rate) = 0;  // powers the codec and the amplifier
    virtual void set_volume(int percent) = 0;
    virtual bool write(const int16_t *samples, size_t count) = 0;  // blocks until played
    virtual void close() = 0;                                      // amplifier off
};

// Bus side runs on the UI loop; tick() runs on the audio task and is the only code that touches the port.
class AudioService {
public:
    static constexpr uint64_t kIdleCloseMs = 3000;  // amplifier off this long after the last sound
    static constexpr uint64_t kAlarmGapMs = 500;    // pause between alarm repeats

    AudioService(EventBus &bus, StateStore &state, Settings &settings, AudioPort &port);
    ~AudioService();

    void tick(uint64_t now_ms);

private:
    bool dnd_now() const;
    void queue(Sound s);
    void play(Sound s, uint64_t now_ms);

    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    AudioPort &port_;
    std::vector<EventBus::SubscriptionId> subs_;

    std::mutex mu_;  // guards the fields below (bus handlers vs the audio task)
    std::vector<Sound> pending_;
    bool ringing_ = false;
    int volume_ = 60;
    int live_volume_ = -1;  // slider being dragged, -1 = none

    bool open_ = false;  // audio task only
    int applied_volume_ = -1;
    uint64_t last_activity_ms_ = 0;
    uint64_t next_alarm_ms_ = 0;
};

}  // namespace summit
