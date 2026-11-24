// Audio service: which sound plays when (pure rules, host-tested in tests/host/test_audio.cpp) and the service that
// turns bus events into sounds through an AudioPort (the ES8311 codec + amplifier on the board; a fake in tests).
//
// Sounds: Preview = a generated tick when the volume slider is released; Warning (a critical toast), UsbPlug (USB power
// gets plugged in) and Alarm (the cowbell, played again while a timer rings) are recordings built into the firmware as
// 8-bit mu-law (tools/make_sounds.py). Paul's rule: the device is quiet; only critical things and alarms make a sound.
// Rules: volume 0 silences everything but the Alarm; do-not-disturb (setting system/dnd or state power/dnd) silences
// UsbPlug only (a Warning is for critical things); an Alarm always rings, never quieter than kAlarmMinVolume, and stops
// the moment it is dismissed, so a timer is never missed and never keeps ringing.
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

enum class Sound { Preview, Warning, UsbPlug, Alarm };

struct Note {
    float hz;  // 0 = silence
    int ms;
};

constexpr int kAudioSampleRate = 16000;  // mono, 16-bit
constexpr int kAlarmMinVolume = 40;      // percent
constexpr int kCodecMinVolume = 35;      // codec volume of slider step 1: the codec's own curve is inaudible below about 30%
constexpr int kTargetPeak = 32000;       // every sound is scaled up to this peak (full scale is 32767)

// A recorded sound: 16 kHz mono 8-bit mu-law (null for Preview, which is generated from sound_notes()).
struct SoundClip {
    const uint8_t *ulaw;
    size_t samples;
};
const SoundClip *sound_clip(Sound s);  // sounds_data.cpp (generated)
int16_t ulaw_decode(uint8_t u);

std::vector<Note> sound_notes(Sound s);  // the generated sounds only (empty for the recordings)
int sound_length_ms(Sound s);            // either kind
// Sine notes with a 5 ms fade in/out so nothing clicks. peak = largest sample (0..32767).
std::vector<int16_t> render_notes(const std::vector<Note> &notes, int sample_rate, int peak);
// Bus events: audio/play (a sound by name, obeys the rules below) and audio/test (the web Diagnostics buttons: same
// names, but do-not-disturb is ignored and a test alarm plays kTestAlarmMs once without a timer ringing).
// Parses the payload of those events: "preview", "warning", "usb" or "alarm".
bool parse_sound(const std::string &name, Sound &out);
bool audible(Sound s, bool dnd, int volume);
// Volume after the rules above (0..100).
int effective_volume(Sound s, int volume);
// Slider/effective volume -> codec volume: 0 stays 0 (silent), 1..100 spreads over kCodecMinVolume..100 so the lowest
// step is audible and the top is as loud as the codec goes.
int codec_volume(int volume);
// Gain (8.8 fixed point, 256 = 1.0) that brings a sound's loudest sample to kTargetPeak.
int sound_gain_q8(Sound s);

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
    static constexpr size_t kTestAlarmMs = 3000;    // the Diagnostics "Test alarm" plays this much of the cowbell

    AudioService(EventBus &bus, StateStore &state, Settings &settings, AudioPort &port);
    ~AudioService();

    void tick(uint64_t now_ms);

private:
    bool dnd_now() const;
    void queue(Sound s, bool test = false);
    bool alarm_ringing();
    bool usb_known_ = false;   // a USB reading has been seen (the first one is the baseline, not a plug-in)
    bool usb_was_on_ = false;  // for the plug-in edge (bus thread only)
    void play(Sound s, uint64_t now_ms, bool test = false);

    EventBus &bus_;
    StateStore &state_;
    Settings &settings_;
    AudioPort &port_;
    std::vector<EventBus::SubscriptionId> subs_;

    std::mutex mu_;  // guards the fields below (bus handlers vs the audio task)
    struct Pending {
        Sound sound;
        bool test;  // from the web Diagnostics buttons: ignores do-not-disturb; a test alarm plays kTestAlarmMs once
    };
    std::vector<Pending> pending_;
    bool ringing_ = false;
    int volume_ = 60;
    int live_volume_ = -1;  // slider being dragged, -1 = none

    bool open_ = false;  // audio task only
    int applied_volume_ = -1;
    uint64_t last_activity_ms_ = 0;
    uint64_t next_alarm_ms_ = 0;
};

}  // namespace summit
