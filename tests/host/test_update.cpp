#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "sha256_ref.h"
#include "summit/update/catalog.h"
#include "summit/update/engine.h"
#include "summit/update/health_gate.h"
#include "summit/update/image_check.h"
#include "summit/update/preflight.h"
#include "summit/update/version.h"

using namespace summit::update;

static Version V(const char *s)
{
    Version v;
    CHECK(parse_version(s, v));
    return v;
}

TEST(sha256_reference_vectors)
{
    Sha256Ref h;
    CHECK(h.hex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    h.reset();
    const char *abc = "abc";
    h.update(reinterpret_cast<const uint8_t *>(abc), 3);
    CHECK(h.hex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    h.reset();
    std::string m(1000, 'a');
    h.update(reinterpret_cast<const uint8_t *>(m.data()), m.size());
    CHECK(h.hex() == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

TEST(semver_parse_and_precedence)
{
    Version v;
    CHECK(parse_version("0.1.0", v) && v.minor == 1);
    CHECK(parse_version("1.2.3-beta.11", v) && v.pre.size() == 2);
    for (const char *bad : {"", "1.2", "1.2.3.4", "01.2.3", "1.2.3-", "1.2.3-beta..1", "1.2.3+build", "v1.2.3",
                            "1.2.3-be ta", "1.2.3-01"})
        CHECK(!parse_version(bad, v));
    const char *order[] = {"0.9.9", "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                           "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.10.0"};
    for (size_t i = 0; i + 1 < sizeof order / sizeof *order; ++i) {
        CHECK(compare_versions(V(order[i]), V(order[i + 1])) < 0);
        CHECK(compare_versions(V(order[i + 1]), V(order[i])) > 0);
    }
    CHECK(compare_versions(V("1.0.0"), V("1.0.0")) == 0);
}

static const char *kHw = "waveshare-p4-wifi6-lcd5-rev3";
static const std::string kSha(64, 'a');

static DeviceInfo device(Channel ch = Channel::Stable)
{
    DeviceInfo d;
    d.hardware_id = kHw;
    d.version = "0.1.0";
    d.slot_size = 0x800000;
    d.channel = ch;
    d.allowed_hosts = {"skiiirt1983.github.io"};
    return d;
}

static CatalogBuild build(const char *version, const char *channel, int64_t size = 300000)
{
    CatalogBuild b;
    b.version = version;
    b.channel = channel;
    b.hardware_id = kHw;
    b.partition_layout = kPartitionLayout;
    b.url = std::string("https://skiiirt1983.github.io/slopeform-home/firmware/") + version + "/summit-ota.bin";
    b.size = size;
    b.sha256 = kSha;
    return b;
}

static Catalog catalog(std::vector<CatalogBuild> b)
{
    Catalog c;
    c.format = kCatalogFormat;
    c.project = "summit";
    c.builds = std::move(b);
    return c;
}

TEST(catalog_header_must_match)
{
    std::string err;
    Catalog c = catalog({});
    CHECK(check_catalog(c, device(), &err));
    c.format = 2;
    CHECK(!check_catalog(c, device(), &err) && err == "unsupported catalog format 2");
    c = catalog({});
    c.project = "lyricflash";
    CHECK(!check_catalog(c, device(), &err));
}

TEST(catalog_rejects_each_bad_field)
{
    auto rejected = [](CatalogBuild b) { return evaluate(catalog({b}), device(Channel::Alpha))[0].reject; };
    CatalogBuild b = build("0.2.0", "stable");
    CHECK(rejected(b) == Reject::None);
    b = build("0.2", "stable");
    CHECK(rejected(b) == Reject::BadVersion);
    b = build("0.2.0", "nightly");
    CHECK(rejected(b) == Reject::BadChannel);
    b = build("0.2.0", "stable");
    b.hardware_id = "waveshare-p4-wifi6-lcd5-rev1";  // other chip-revision family
    CHECK(rejected(b) == Reject::WrongHardware);
    b = build("0.2.0", "stable");
    b.partition_layout = 2;
    CHECK(rejected(b) == Reject::WrongLayout);
    for (const char *u : {"http://skiiirt1983.github.io/x.bin", "https://skiiirt1983.github.io",
                          "https://user@skiiirt1983.github.io/x.bin", "https://skiiirt1983.github.io:444/x.bin",
                          "https://SKIIIRT1983.github.io/x.bin", "https://skiiirt1983.github.io/a b.bin"}) {
        b = build("0.2.0", "stable");
        b.url = u;
        CHECK(rejected(b) == Reject::BadUrl);
    }
    b = build("0.2.0", "stable");
    b.url = "https://evil.example.com/summit.bin";
    CHECK(rejected(b) == Reject::HostNotAllowed);
    b = build("0.2.0", "stable", 0x800001);
    CHECK(rejected(b) == Reject::BadSize);
    b = build("0.2.0", "stable");
    b.sha256 = std::string(64, 'A');
    CHECK(rejected(b) == Reject::BadSha256);
}

TEST(channels_and_newest_update)
{
    Catalog c = catalog({build("0.1.0", "stable"), build("0.2.0", "stable"), build("0.3.0-beta.1", "beta"),
                         build("0.3.0-alpha.4", "alpha"), build("0.0.9", "stable")});
    auto newest = [&](Channel ch) {
        auto cands = evaluate(c, device(ch));
        const Candidate *n = newest_update(cands, V("0.1.0"));
        return n ? n->build.version : std::string("none");
    };
    CHECK(newest(Channel::Stable) == "0.2.0");
    CHECK(newest(Channel::Beta) == "0.3.0-beta.1");
    CHECK(newest(Channel::Alpha) == "0.3.0-beta.1");  // beta.1 > alpha.4 by SemVer
    auto cands = evaluate(c, device(Channel::Stable));
    CHECK(newest_update(cands, V("0.2.0")) == nullptr);  // up to date
    CHECK(cands[4].reject == Reject::None);              // older builds stay installable (rollback by choice)
}

TEST(preflight_rules)
{
    PreflightInput in;
    in.on_usb_power = false;
    in.battery_pct = 39;
    in.next_alarm_in_s = 14 * 60;
    in.wifi_connected = false;
    in.time_synced = false;
    in.image_size = 0x800001;
    in.slot_size = 0x800000;
    CHECK(preflight(in) == (kNeedPower | kAlarmSoon | kNoWifi | kNoTime | kDoesNotFit));
    in.battery_pct = 40;
    in.next_alarm_in_s = 15 * 60;
    in.wifi_connected = true;
    in.time_synced = true;
    in.image_size = 0x800000;
    CHECK(preflight(in) == 0);
    in.battery_pct = 5;
    in.on_usb_power = true;
    in.next_alarm_in_s = -1;
    CHECK(preflight(in) == 0);
    in.time_synced = false;
    in.from_network = false;  // web upload: no TLS, no clock needed
    CHECK(preflight(in) == 0);
    CHECK(preflight_message(kNeedPower) == "Connect USB power or charge to 40% first");
    // D-011: a running recipe timer blocks installs, uploads and going back a version.
    CHECK(previous_version_blocker(in).empty());
    in.timer_running = true;
    CHECK(preflight(in) == kTimerRunning);
    CHECK(preflight_message(kTimerRunning) == "Finish or cancel your timers first");
    CHECK(previous_version_blocker(in) == "Finish or cancel your timers first");
    in.timer_running = false;
}

// ---- fake image + writer ----------------------------------------------------

static std::vector<uint8_t> image(const char *version, size_t size = 4096, uint16_t min_rev = 301,
                                  uint16_t max_rev = 399, const char *project = "summit")
{
    std::vector<uint8_t> b(size);
    for (size_t i = 0; i < size; ++i) b[i] = static_cast<uint8_t>(i * 7);
    std::memset(b.data(), 0, 0x70);
    b[0] = 0xE9;
    b[12] = 0x12;
    b[13] = 0;
    b[15] = min_rev & 0xFF;
    b[16] = min_rev >> 8;
    b[17] = max_rev & 0xFF;
    b[18] = max_rev >> 8;
    const uint8_t magic[4] = {0x32, 0x54, 0xCD, 0xAB};
    std::memcpy(&b[0x20], magic, 4);
    std::strncpy(reinterpret_cast<char *>(&b[0x30]), version, 31);
    std::strncpy(reinterpret_cast<char *>(&b[0x50]), project, 31);
    return b;
}

static std::string sha(const std::vector<uint8_t> &b)
{
    Sha256Ref h;
    h.update(b.data(), b.size());
    return h.hex();
}

struct FakeWriter : OtaWriter {
    std::vector<uint8_t> flash;
    bool open = false, finished = false, aborted = false, fail_finish = false;
    size_t expected = 0;
    bool begin(size_t n, std::string *) override
    {
        open = true;
        expected = n;
        flash.clear();
        return true;
    }
    bool write(const uint8_t *p, size_t n, std::string *) override
    {
        flash.insert(flash.end(), p, p + n);
        return true;
    }
    bool finish(std::string *e) override
    {
        open = false;
        if (fail_finish) {
            *e = "image hash invalid";
            return false;
        }
        finished = true;
        return true;
    }
    void abort() override
    {
        open = false;
        aborted = true;
    }
};

struct RefHasher : Hasher {
    Sha256Ref h;
    void reset() override { h.reset(); }
    void update(const uint8_t *p, size_t n) override { h.update(p, n); }
    std::string hex_digest() override { return h.hex(); }
};

static PreflightInput ok_preflight()
{
    PreflightInput in;
    in.on_usb_power = true;
    in.wifi_connected = true;
    in.time_synced = true;
    return in;
}

struct Rig {
    FakeWriter writer;
    RefHasher hasher;
    UpdateEngine engine;
    explicit Rig(const char *family = "rev3") : engine(device(), expect(family), writer, hasher) {}
    static ImageExpect expect(const char *family)
    {
        ImageExpect e;
        expect_for_family(family, e);
        return e;
    }
    void feed(const std::vector<uint8_t> &img, size_t chunk = 1000)
    {
        for (size_t i = 0; i < img.size(); i += chunk)
            if (!engine.data(img.data() + i, std::min(chunk, img.size() - i))) return;
    }
};

TEST(catalog_install_happy_path)
{
    Rig r;
    auto img = image("0.2.0");
    CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    b.sha256 = sha(img);
    CHECK(r.engine.begin_check());
    r.engine.catalog_received(catalog({b}));
    CHECK(r.engine.status().stage == Stage::Available && r.engine.available()->build.version == "0.2.0");
    CHECK(r.engine.start_install("0.2.0", ok_preflight()));
    CHECK(r.engine.busy() && !r.engine.begin_check());          // one update at a time
    CHECK(!r.engine.start_upload(100, ok_preflight()));         // web upload refused while busy
    r.feed(img, 33);                                            // header split across chunks
    CHECK(r.engine.status().bytes == static_cast<int64_t>(img.size()));
    CHECK(r.engine.complete());
    CHECK(r.engine.status().stage == Stage::ReadyToReboot && r.writer.finished && r.writer.flash == img);
}

TEST(writing_stage_shows_before_the_flash_burst)
{
    Rig r;
    auto img = image("0.2.0");
    CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    b.sha256 = sha(img);
    CHECK(r.engine.begin_check());
    r.engine.catalog_received(catalog({b}));
    CHECK(r.engine.start_install("0.2.0", ok_preflight()));
    CHECK(!r.engine.begin_writing());                              // nothing downloaded yet
    r.feed(img);
    CHECK(r.engine.begin_writing());                               // download done: "Writing update" stage
    CHECK(r.engine.status().stage == Stage::Verifying && r.engine.busy());  // still counts as installing
    CHECK(!r.writer.finished);                                     // the flash burst has not happened yet
    CHECK(r.engine.complete());                                    // complete() accepts the writing stage
    CHECK(r.engine.status().stage == Stage::ReadyToReboot && r.writer.finished && r.writer.flash == img);
}

TEST(writing_stage_still_rejects_a_bad_hash_and_a_short_download)
{
    Rig r;
    auto img = image("0.2.0");
    CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    b.sha256 = std::string(64, 'b');                               // wrong hash
    CHECK(r.engine.begin_check());
    r.engine.catalog_received(catalog({b}));
    CHECK(r.engine.start_install("0.2.0", ok_preflight()));
    r.feed(img);
    CHECK(r.engine.begin_writing());
    CHECK(!r.engine.complete() && r.engine.status().stage == Stage::Failed);
    CHECK(r.writer.aborted && !r.writer.finished);

    Rig t;
    CatalogBuild b2 = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    b2.sha256 = sha(img);
    CHECK(t.engine.begin_check());
    t.engine.catalog_received(catalog({b2}));
    CHECK(t.engine.start_install("0.2.0", ok_preflight()));
    t.feed(std::vector<uint8_t>(img.begin(), img.begin() + 2000));
    CHECK(!t.engine.begin_writing());                              // short download never reaches the writing stage
}

TEST(install_picks_this_devices_entry_when_a_version_lists_both_chip_variants)
{
    // Live bug (2026-11-22): the release catalog has a rev3 and a rev1 entry under the same version. The device
    // downloaded the last one (rev1) and the rev3 image check rejected it. Either order must pick the rev3 entry.
    for (int rev1_first = 0; rev1_first < 2; ++rev1_first) {
        Rig r;
        auto img = image("0.2.0");
        CatalogBuild mine = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
        mine.sha256 = sha(img);
        mine.url = "https://skiiirt1983.github.io/slopeform-home/firmware/0.2.0/summit-0.2.0-rev3-ota.bin";
        CatalogBuild other = build("0.2.0", "stable", static_cast<int64_t>(img.size()) - 7);
        other.hardware_id = "waveshare-p4-wifi6-lcd5-rev1";
        other.url = "https://skiiirt1983.github.io/slopeform-home/firmware/0.2.0/summit-0.2.0-rev1-ota.bin";
        CHECK(r.engine.begin_check());
        r.engine.catalog_received(catalog(rev1_first ? std::vector<CatalogBuild>{other, mine}
                                                     : std::vector<CatalogBuild>{mine, other}));
        const Candidate *c = r.engine.installable("0.2.0");
        CHECK(c && c->build.url == mine.url && c->build.hardware_id == kHw);
        CHECK(r.engine.start_install("0.2.0", ok_preflight()));
        r.feed(img, 4096);
        CHECK(r.engine.complete() && r.engine.status().stage == Stage::ReadyToReboot);
        CHECK(r.engine.installable("9.9.9") == nullptr);
    }
    // Only the other chip's entry exists: refused, with the reason.
    Rig r;
    CatalogBuild other = build("0.2.0", "stable");
    other.hardware_id = "waveshare-p4-wifi6-lcd5-rev1";
    CHECK(r.engine.begin_check());
    r.engine.catalog_received(catalog({other}));
    CHECK(r.engine.installable("0.2.0") == nullptr && !r.engine.start_install("0.2.0", ok_preflight()));
    CHECK(r.engine.status().stage == Stage::Failed);
}

TEST(catalog_install_rejects_bad_images_before_writing)
{
    struct Case {
        std::vector<uint8_t> img;
        const char *why;
    };
    std::vector<Case> cases = {
        {image("0.2.0", 4096, 101, 199), "different chip revision"},
        {image("0.2.0", 4096, 301, 399, "lyricflash"), "not Project Summit"},
        {image("0.3.0"), "version does not match"},
    };
    for (auto &c : cases) {
        Rig r;
        CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(c.img.size()));
        b.sha256 = sha(c.img);
        r.engine.begin_check();
        r.engine.catalog_received(catalog({b}));
        CHECK(r.engine.start_install("0.2.0", ok_preflight()));
        r.feed(c.img);
        CHECK(r.engine.status().stage == Stage::Failed);
        CHECK(r.engine.status().error.find(c.why) != std::string::npos);
        CHECK(r.writer.flash.empty() && r.writer.aborted);  // nothing written
    }
}

TEST(catalog_install_sha_mismatch_and_truncation)
{
    Rig r;
    auto img = image("0.2.0");
    CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    b.sha256 = std::string(64, 'b');
    r.engine.begin_check();
    r.engine.catalog_received(catalog({b}));
    r.engine.start_install("0.2.0", ok_preflight());
    r.feed(img);
    CHECK(!r.engine.complete() && r.engine.status().error == "SHA-256 does not match the catalog");
    CHECK(r.writer.aborted && !r.writer.finished);

    Rig t;
    b.sha256 = sha(img);
    t.engine.begin_check();
    t.engine.catalog_received(catalog({b}));
    t.engine.start_install("0.2.0", ok_preflight());
    t.engine.data(img.data(), 2000);
    t.engine.fail("connection reset", 0);
    CHECK(t.engine.status().stage == Stage::Failed && t.engine.status().error == "connection reset");
    CHECK(t.writer.aborted);
    CHECK(!t.engine.complete());
}

TEST(install_blocked_by_preflight_or_rejection)
{
    Rig r;
    auto img = image("0.2.0");
    CatalogBuild b = build("0.2.0", "stable", static_cast<int64_t>(img.size()));
    CatalogBuild other = build("0.3.0", "stable");
    other.hardware_id = "waveshare-p4-wifi6-lcd5-rev1";
    r.engine.begin_check();
    r.engine.catalog_received(catalog({b, other}));
    PreflightInput low = ok_preflight();
    low.on_usb_power = false;
    low.battery_pct = 20;
    CHECK(!r.engine.start_install("0.2.0", low));
    CHECK(r.engine.status().preflight_failures == kNeedPower && !r.writer.open);
    CHECK(!r.engine.start_install("0.3.0", ok_preflight()));
    CHECK(r.engine.status().error == "cannot install 0.3.0: built for different hardware");
    CHECK(!r.engine.start_install("9.9.9", ok_preflight()));
}

TEST(web_upload_uses_same_engine)
{
    Rig r;
    auto img = image("0.4.0-beta.1");
    PreflightInput in = ok_preflight();
    in.time_synced = false;  // not needed for uploads
    CHECK(r.engine.start_upload(static_cast<int64_t>(img.size()), in));
    r.feed(img, 4096);
    CHECK(r.engine.complete());
    CHECK(r.engine.status().source == Source::Upload && r.engine.status().target_version == "0.4.0-beta.1");
    CHECK(r.engine.status().sha256 == sha(img));

    Rig rev1("rev1");  // a rev1 device refuses a rev3 upload
    CHECK(rev1.engine.start_upload(static_cast<int64_t>(img.size()), in));
    rev1.feed(img);
    CHECK(rev1.engine.status().error == "rejected image: built for a different chip revision");

    Rig big;
    CHECK(!big.engine.start_upload(0x800001, in) && big.engine.status().preflight_failures == kDoesNotFit);

    Rig over;
    over.engine.start_upload(100, in);
    CHECK(!over.engine.data(img.data(), 200) && over.engine.status().error == "received more data than expected");

    Rig bad;
    bad.writer.fail_finish = true;
    bad.engine.start_upload(static_cast<int64_t>(img.size()), in);
    bad.feed(img);
    CHECK(!bad.engine.complete() && bad.engine.status().error == "image validation failed: image hash invalid");
}

TEST(check_failures_and_up_to_date)
{
    Rig r;
    r.engine.begin_check();
    r.engine.check_failed("TLS handshake failed", 0);
    CHECK(r.engine.status().stage == Stage::CheckFailed);
    r.engine.begin_check();
    Catalog c = catalog({build("0.1.0", "stable")});
    c.format = 7;
    r.engine.catalog_received(c);
    CHECK(r.engine.status().stage == Stage::CheckFailed &&
          r.engine.status().error == "unsupported catalog format 7");
    r.engine.begin_check();
    r.engine.catalog_received(catalog({build("0.1.0", "stable")}));
    CHECK(r.engine.status().stage == Stage::UpToDate && !r.engine.available());
}

TEST(health_gate_three_consecutive_samples_within_40s)
{
    HealthSample good;
    good.ui_tick_age_ms = 100;
    good.frames_rendered = 10;
    good.free_heap = 100000;
    good.touch_ok = good.i2c_ok = good.settings_ok = true;
    CHECK(sample_healthy(good));
    HealthSample bad = good;
    bad.touch_ok = false;
    CHECK(!sample_healthy(bad));
    for (auto mutate : {+[](HealthSample &s) { s.ui_tick_age_ms = 1501; }, +[](HealthSample &s) { s.frames_rendered = 0; },
                        +[](HealthSample &s) { s.free_heap = 16383; }, +[](HealthSample &s) { s.i2c_ok = false; },
                        +[](HealthSample &s) { s.settings_ok = false; }}) {
        HealthSample s = good;
        mutate(s);
        CHECK(!sample_healthy(s));
    }

    HealthGate g;
    CHECK(g.feed(good, 2500) == GateDecision::Wait);
    CHECK(g.feed(good, 5000) == GateDecision::Wait);
    CHECK(g.feed(bad, 7500) == GateDecision::Wait && g.consecutive() == 0);  // streak resets
    CHECK(g.feed(good, 10000) == GateDecision::Wait);
    CHECK(g.feed(good, 12500) == GateDecision::Wait);
    CHECK(g.feed(good, 15000) == GateDecision::Confirm);
    CHECK(g.feed(bad, 17500) == GateDecision::Confirm);  // decision is final

    HealthGate slow;
    for (uint32_t t = 2500; t < 40000; t += 2500) CHECK(slow.feed(t % 5000 ? good : bad, t) == GateDecision::Wait);
    CHECK(slow.feed(good, 40000) == GateDecision::Rollback);

    HealthGate fast;
    CHECK(fast.init_failed() == GateDecision::Rollback && fast.feed(good, 2500) == GateDecision::Rollback);
}

TEST(i2c_only_built_in_devices_gate_health)
{
    auto devices = board_i2c_devices();
    for (auto &d : devices) d.present = true;
    std::vector<std::string> warnings;
    CHECK(i2c_healthy(devices, &warnings) && warnings.empty());

    // Accelerometer missing -> still healthy, one diagnostics warning.
    auto no_accel = devices;
    for (auto &d : no_accel)
        if (d.name == "LIS2DW12 accelerometer") d.present = false;
    warnings.clear();
    CHECK(i2c_healthy(no_accel, &warnings));
    CHECK(warnings.size() == 1 && warnings[0].find("LIS2DW12") != std::string::npos);

    HealthSample s;
    s.ui_tick_age_ms = 100;
    s.frames_rendered = 1;
    s.free_heap = 100000;
    s.touch_ok = s.settings_ok = true;
    s.i2c_ok = i2c_healthy(no_accel, nullptr);
    HealthGate g;
    g.feed(s, 2500);
    g.feed(s, 5000);
    CHECK(g.feed(s, 7500) == GateDecision::Confirm);  // a new image is confirmed without the accelerometer

    // A built-in device missing (codec) -> unhealthy.
    auto no_codec = devices;
    for (auto &d : no_codec)
        if (d.name == "ES8311 codec") d.present = false;
    CHECK(!i2c_healthy(no_codec, nullptr));
}

int main()
{
    RUN(sha256_reference_vectors);
    RUN(semver_parse_and_precedence);
    RUN(catalog_header_must_match);
    RUN(catalog_rejects_each_bad_field);
    RUN(channels_and_newest_update);
    RUN(preflight_rules);
    RUN(catalog_install_happy_path);
    RUN(writing_stage_shows_before_the_flash_burst);
    RUN(writing_stage_still_rejects_a_bad_hash_and_a_short_download);
    RUN(install_picks_this_devices_entry_when_a_version_lists_both_chip_variants);
    RUN(catalog_install_rejects_bad_images_before_writing);
    RUN(catalog_install_sha_mismatch_and_truncation);
    RUN(install_blocked_by_preflight_or_rejection);
    RUN(web_upload_uses_same_engine);
    RUN(check_failures_and_up_to_date);
    RUN(health_gate_three_consecutive_samples_within_40s);
    RUN(i2c_only_built_in_devices_gate_health);
    return report("update engine");
}
