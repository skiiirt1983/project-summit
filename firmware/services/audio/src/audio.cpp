#include "summit/audio.h"

#include <algorithm>
#include <cmath>

namespace summit {

std::vector<Note> sound_notes(Sound s)
{
    if (s == Sound::Preview) return {{1800.f, 30}};
    return {};
}

int16_t ulaw_decode(uint8_t u)
{
    u = static_cast<uint8_t>(~u);
    int t = ((u & 0x0F) << 3) + 0x84;
    t <<= (u & 0x70) >> 4;
    return static_cast<int16_t>((u & 0x80) ? (0x84 - t) : (t - 0x84));
}

int sound_length_ms(Sound s)
{
    if (const SoundClip *c = sound_clip(s)) return static_cast<int>(c->samples * 1000 / kAudioSampleRate);
    int total = 0;
    for (const Note &n : sound_notes(s)) total += n.ms;
    return total;
}

std::vector<int16_t> render_notes(const std::vector<Note> &notes, int sample_rate, int peak)
{
    std::vector<int16_t> out;
    if (sample_rate <= 0) return out;
    peak = std::clamp(peak, 0, 32767);
    const int fade = std::max(1, sample_rate * 5 / 1000);
    constexpr double kTwoPi = 6.283185307179586;
    for (const Note &n : notes) {
        const int count = std::max(0, n.ms) * sample_rate / 1000;
        for (int i = 0; i < count; ++i) {
            if (n.hz <= 0.f) {
                out.push_back(0);
                continue;
            }
            double env = 1.0;
            if (i < fade) env = static_cast<double>(i) / fade;
            if (count - 1 - i < fade) env = std::min(env, static_cast<double>(count - 1 - i) / fade);
            out.push_back(static_cast<int16_t>(std::lround(std::sin(kTwoPi * n.hz * i / sample_rate) * env * peak)));
        }
    }
    return out;
}

bool parse_sound(const std::string &name, Sound &out)
{
    if (name == "preview") out = Sound::Preview;
    else if (name == "warning") out = Sound::Warning;
    else if (name == "usb") out = Sound::UsbPlug;
    else if (name == "alarm") out = Sound::Alarm;
    else return false;
    return true;
}

bool audible(Sound s, bool dnd, int volume)
{
    switch (s) {
    case Sound::Alarm: return true;
    case Sound::UsbPlug: return volume > 0 && !dnd;
    case Sound::Warning: return volume > 0;  // critical: do-not-disturb does not matter
    case Sound::Preview: return volume > 0;  // the user is adjusting it: do-not-disturb does not matter
    }
    return false;
}

int effective_volume(Sound s, int volume)
{
    volume = std::clamp(volume, 0, 100);
    return s == Sound::Alarm ? std::max(volume, kAlarmMinVolume) : volume;
}

int codec_volume(int volume)
{
    volume = std::clamp(volume, 0, 100);
    if (volume == 0) return 0;
    return kCodecMinVolume + (volume - 1) * (100 - kCodecMinVolume) / 99;
}

int sound_gain_q8(Sound s)
{
    static int cache[4] = {0, 0, 0, 0};
    int &g = cache[static_cast<int>(s)];
    if (g) return g;
    int peak = 1;
    if (const SoundClip *c = sound_clip(s)) {
        for (size_t i = 0; i < c->samples; ++i) peak = std::max(peak, std::abs(static_cast<int>(ulaw_decode(c->ulaw[i]))));
    } else {
        for (int16_t v : render_notes(sound_notes(s), kAudioSampleRate, kTargetPeak)) peak = std::max(peak, std::abs(static_cast<int>(v)));
    }
    g = std::max(256, kTargetPeak * 256 / peak);  // never quieter than the recording
    return g;
}

namespace {
int int_of(const Value &v, int fallback)
{
    if (const int64_t *i = std::get_if<int64_t>(&v)) return static_cast<int>(std::clamp<int64_t>(*i, 0, 100));
    return fallback;
}
bool bool_of(const Value &v)
{
    const bool *b = std::get_if<bool>(&v);
    return b && *b;
}
}  // namespace

AudioService::AudioService(EventBus &bus, StateStore &state, Settings &settings, AudioPort &port)
    : bus_(bus), state_(state), settings_(settings), port_(port)
{
    volume_ = int_of(settings_.get("audio/volume"), 60);
    subs_.push_back(bus_.subscribe("audio/play", [this](const std::string &, const Value &v) {
        const std::string *name = std::get_if<std::string>(&v);
        Sound s;
        if (name && parse_sound(*name, s)) queue(s);
    }));
    subs_.push_back(bus_.subscribe("audio/test", [this](const std::string &, const Value &v) {
        const std::string *name = std::get_if<std::string>(&v);
        Sound s;
        if (name && parse_sound(*name, s)) queue(s, true);
    }));
    subs_.push_back(bus_.subscribe("ui/volume_live", [this](const std::string &, const Value &v) {
        std::lock_guard<std::mutex> lock(mu_);
        live_volume_ = int_of(v, volume_);
    }));
    // Slider released: the new volume is saved, so play a short tick at it.
    subs_.push_back(bus_.subscribe("settings/audio/volume", [this](const std::string &, const Value &v) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            volume_ = int_of(v, volume_);
            live_volume_ = -1;
        }
        queue(Sound::Preview);
    }));
    // A critical toast (shown even full-screen) plays the warning hit; nothing else makes a sound on its own.
    subs_.push_back(bus_.subscribe("notify/toast_critical", [this](const std::string &, const Value &) { queue(Sound::Warning); }));
    // USB power plugged in (off -> on); the value at start-up is not a plug-in.
    usb_known_ = std::holds_alternative<bool>(state_.get("power/usb_power"));
    usb_was_on_ = bool_of(state_.get("power/usb_power"));
    subs_.push_back(bus_.subscribe("power/usb_power", [this](const std::string &, const Value &v) {
        const bool on = bool_of(v);
        if (usb_known_ && on && !usb_was_on_) queue(Sound::UsbPlug);
        usb_known_ = true;
        usb_was_on_ = on;
    }));
    subs_.push_back(bus_.subscribe("timers/ringing", [this](const std::string &, const Value &v) {
        std::lock_guard<std::mutex> lock(mu_);
        ringing_ = bool_of(v);
    }));
    ringing_ = bool_of(state_.get("timers/ringing"));
}

AudioService::~AudioService()
{
    for (auto id : subs_) bus_.unsubscribe(id);
}

bool AudioService::dnd_now() const { return bool_of(settings_.get("system/dnd")) || bool_of(state_.get("power/dnd")); }

void AudioService::queue(Sound s, bool test)
{
    const bool dnd = test ? false : dnd_now();  // a test button press is the user asking: do-not-disturb does not matter
    std::lock_guard<std::mutex> lock(mu_);
    if (!audible(s, dnd, live_volume_ >= 0 ? live_volume_ : volume_)) return;
    if (pending_.size() < 4) pending_.push_back({s, test});
}

bool AudioService::alarm_ringing()
{
    std::lock_guard<std::mutex> lock(mu_);
    return ringing_;
}

void AudioService::play(Sound s, uint64_t now_ms, bool test)
{
    int vol;
    {
        std::lock_guard<std::mutex> lock(mu_);
        vol = effective_volume(s, live_volume_ >= 0 ? live_volume_ : volume_);
    }
    if (!open_) {
        if (!port_.open(kAudioSampleRate)) return;
        open_ = true;
        applied_volume_ = -1;
    }
    if (vol != applied_volume_) {
        port_.set_volume(codec_volume(vol));
        applied_volume_ = vol;
    }
    // Idle clock: the amplifier may go off kIdleCloseMs after the sound ends, not after it starts.
    int length_ms = sound_length_ms(s);
    if (test && s == Sound::Alarm) length_ms = std::min<int>(length_ms, static_cast<int>(kTestAlarmMs));
    last_activity_ms_ = now_ms + static_cast<uint64_t>(length_ms);
    const int gain = sound_gain_q8(s);
    if (const SoundClip *clip = sound_clip(s)) {
        // A real alarm stops when dismissed; a test alarm (nothing is ringing) plays its first kTestAlarmMs.
        const size_t limit = (test && s == Sound::Alarm) ? std::min<size_t>(clip->samples, kTestAlarmMs * kAudioSampleRate / 1000) : clip->samples;
        int16_t buf[1024];  // decoded a piece at a time; the alarm stops between pieces once it is dismissed
        for (size_t at = 0; at < limit;) {
            const size_t n = std::min<size_t>(1024, limit - at);
            for (size_t i = 0; i < n; ++i)
                buf[i] = static_cast<int16_t>(std::clamp(ulaw_decode(clip->ulaw[at + i]) * gain / 256, -32767, 32767));
            if (!port_.write(buf, n)) break;
            at += n;
            if (s == Sound::Alarm && !test && !alarm_ringing()) break;
        }
    } else {
        const std::vector<int16_t> pcm = render_notes(sound_notes(s), kAudioSampleRate, kTargetPeak);
        port_.write(pcm.data(), pcm.size());
    }
}

void AudioService::tick(uint64_t now_ms)
{
    std::vector<Pending> todo;
    bool ringing;
    {
        std::lock_guard<std::mutex> lock(mu_);
        todo.swap(pending_);
        ringing = ringing_;
    }
    for (const Pending &p : todo) {
        if (p.sound == Sound::Warning && !p.test && ringing) continue;  // "Timer done" is a critical toast too: the alarm is already ringing
        play(p.sound, now_ms, p.test);
    }
    if (ringing) {
        if (now_ms >= next_alarm_ms_) {
            play(Sound::Alarm, now_ms);
            if (!alarm_ringing()) return;  // dismissed while it played
            next_alarm_ms_ = now_ms + static_cast<uint64_t>(sound_length_ms(Sound::Alarm)) + kAlarmGapMs;
        }
    } else {
        next_alarm_ms_ = 0;
    }
    if (open_ && now_ms > last_activity_ms_ && now_ms - last_activity_ms_ > kIdleCloseMs) {  // last_activity is the end of the sound
        port_.close();
        open_ = false;
    }
}

}  // namespace summit
