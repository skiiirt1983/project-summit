#include "fake_update.h"

#include <cstring>

#include "sha256_ref.h"
#include "summit/update/status_topics.h"

namespace sim {

using namespace summit;
using namespace summit::update;

struct FakeUpdateService::Writer : OtaWriter {
    std::vector<uint8_t> flash;
    bool begin(size_t n, std::string *) override
    {
        flash.clear();
        flash.reserve(n);
        return true;
    }
    bool write(const uint8_t *p, size_t n, std::string *) override
    {
        flash.insert(flash.end(), p, p + n);
        return true;
    }
    bool finish(std::string *) override { return true; }
    void abort() override { flash.clear(); }
};

struct FakeUpdateService::Hash : Hasher {
    Sha256Ref h;
    void reset() override { h.reset(); }
    void update(const uint8_t *p, size_t n) override { h.update(p, n); }
    std::string hex_digest() override { return h.hex(); }
};

namespace {

constexpr const char *kHw = "waveshare-p4-wifi6-lcd5-rev3";
constexpr size_t kChunk = 64 * 1024;

std::vector<uint8_t> make_image(const char *version, size_t size)
{
    std::vector<uint8_t> b(size);
    for (size_t i = 0; i < size; ++i) b[i] = static_cast<uint8_t>(i * 13);
    std::memset(b.data(), 0, 0x70);
    b[0] = 0xE9;
    b[12] = 0x12;
    const uint16_t min_rev = 301, max_rev = 399;
    b[15] = min_rev & 0xFF;
    b[16] = min_rev >> 8;
    b[17] = max_rev & 0xFF;
    b[18] = max_rev >> 8;
    const uint8_t magic[4] = {0x32, 0x54, 0xCD, 0xAB};
    std::memcpy(&b[0x20], magic, 4);
    std::strncpy(reinterpret_cast<char *>(&b[0x30]), version, 31);
    std::strncpy(reinterpret_cast<char *>(&b[0x50]), "summit", 31);
    return b;
}

std::string sha(const std::vector<uint8_t> &b)
{
    Sha256Ref h;
    h.update(b.data(), b.size());
    return h.hex();
}

}  // namespace

static PreflightInput preflight_from(StateStore &st)
{
    auto b = [&](const char *k) { Value x = st.get(k); return std::get_if<bool>(&x) && std::get<bool>(x); };
    Value bat = st.get("power/battery_pct");
    PreflightInput in;
    in.on_usb_power = b("power/usb_power");
    in.battery_pct = std::get_if<int64_t>(&bat) ? static_cast<int>(std::get<int64_t>(bat)) : 0;
    in.wifi_connected = b("net/wifi_connected");
    in.time_synced = b("time/synced");
    Value timers = st.get("timers/list");
    const std::string *list = std::get_if<std::string>(&timers);
    in.timer_running = list && !list->empty();
    return in;
}

FakeUpdateService::FakeUpdateService(EventBus &bus, StateStore &state, Settings &settings, const std::string &version)
    : bus_(bus), state_(state), settings_(settings), writer_(std::make_unique<Writer>()), hash_(std::make_unique<Hash>())
{
    DeviceInfo d;
    d.hardware_id = kHw;
    d.version = version;
    d.slot_size = 0x800000;
    d.allowed_hosts = {"skiiirt1983.github.io"};
    ImageExpect e;
    expect_for_family("rev3", e);
    engine_ = std::make_unique<UpdateEngine>(d, e, *writer_, *hash_);

    struct Spec {
        const char *version, *channel, *hw, *notes;
        size_t size;
    };
    const Spec specs[] = {
        {"0.1.0", "stable", kHw, "Glance strip, quick settings and rotation lock. Faster page swipes.", 1200 * 1024},
        {"0.2.0-beta.1", "beta", kHw, "Beta: weather panel with hourly forecast.", 1300 * 1024},
        {"0.1.0", "stable", "waveshare-p4-wifi6-lcd5-rev1", "Same release for v1.x chips.", 1200 * 1024},
    };
    catalog_.format = kCatalogFormat;
    catalog_.project = "summit";
    images_.reserve(3);
    for (const Spec &s : specs) {
        images_.push_back(make_image(s.version, s.size));
        CatalogBuild b;
        b.version = s.version;
        b.channel = s.channel;
        b.hardware_id = s.hw;
        b.partition_layout = kPartitionLayout;
        b.url = std::string("https://skiiirt1983.github.io/slopeform-home/firmware/") + s.version + "/summit-ota.bin";
        b.size = static_cast<int64_t>(s.size);
        b.sha256 = sha(images_.back());
        b.released = "2026-11-07";
        b.notes = s.notes;
        catalog_.builds.push_back(b);
    }

    // Like the device service: a channel change re-checks.
    channel_sub_ = bus_.subscribe("settings/update/channel", [this](const std::string &, const Value &) {
        bus_.publish("update/command", std::string("check"));
    });
    sub_ = bus_.subscribe("update/command", [this](const std::string &, const Value &v) {
        const std::string *s = std::get_if<std::string>(&v);
        if (!s) return;
        if (*s == "check") {
            Value ch = settings_.get("update/channel");
            Channel c = Channel::Stable;
            if (auto cs = std::get_if<std::string>(&ch)) parse_channel(*cs, c);
            engine_->set_channel(c);
            if (engine_->begin_check()) {
                checking_ = true;
                check_due_ = now_ + 300;  // simulated network latency
            }
        } else if (s->rfind("install:", 0) == 0) {
            const std::string version = s->substr(8);
            PreflightInput in = preflight_from(state_);
            if (engine_->start_install(version, in)) {
                for (size_t i = 0; i < catalog_.builds.size(); ++i)
                    if (catalog_.builds[i].version == version && catalog_.builds[i].hardware_id == kHw)
                        download_ = &images_[i];
                offset_ = 0;
            }
        } else if (*s == "reboot" && engine_->status().stage == Stage::ReadyToReboot) {
            // A real restart: the new version runs with a fresh engine (after the health gate).
            const std::string v = engine_->status().target_version;
            DeviceInfo d = engine_->device();
            d.version = v;
            ImageExpect e;
            expect_for_family("rev3", e);
            engine_ = std::make_unique<UpdateEngine>(d, e, *writer_, *hash_);
            state_.set("system/firmware_version", v);
            bus_.publish("notify/toast", "Updated to " + v);
        } else if (*s == "previous") {
            const std::string blocker = previous_version_blocker(preflight_from(state_));
            bus_.publish("notify/toast", blocker.empty() ? std::string("Switching to the previous version") : blocker);
        }
        publish_status(*engine_, state_);
    });
    publish_status(*engine_, state_);
}

FakeUpdateService::~FakeUpdateService()
{
    bus_.unsubscribe(sub_);
    bus_.unsubscribe(channel_sub_);
}


bool FakeUpdateService::upload_begin(int64_t size, std::string *error)
{
    bool ok = engine_->start_upload(size, preflight_from(state_));
    if (!ok && error) *error = engine_->status().error;
    publish_status(*engine_, state_);
    return ok;
}

bool FakeUpdateService::upload_data(const uint8_t *data, size_t len)
{
    bool ok = engine_->data(data, len);
    publish_status(*engine_, state_);
    return ok;
}

bool FakeUpdateService::upload_complete(std::string *error)
{
    bool ok = engine_->complete();
    if (!ok && error) *error = engine_->status().error;
    publish_status(*engine_, state_);
    return ok;
}

void FakeUpdateService::upload_abort(const char *reason)
{
    engine_->fail(reason);
    publish_status(*engine_, state_);
}

void FakeUpdateService::tick(uint32_t now_ms)
{
    now_ = now_ms;
    if (checking_ && static_cast<int32_t>(now_ - check_due_) >= 0) {
        checking_ = false;
        engine_->catalog_received(catalog_);
        publish_status(*engine_, state_);
    }
    if (download_ && engine_->busy()) {
        size_t n = std::min(kChunk, download_->size() - offset_);
        engine_->data(download_->data() + offset_, n);
        offset_ += n;
        if (offset_ >= download_->size()) {
            engine_->complete();
            download_ = nullptr;
        }
        publish_status(*engine_, state_);
    }
}

}  // namespace sim
