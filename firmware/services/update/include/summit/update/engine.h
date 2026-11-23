// The one update engine (hard rule 5): the on-device Updates screen and the web UI
// both drive this. One update at a time. Pure logic; flash writes and hashing go
// through OtaWriter / Hasher (ESP-IDF adapters on the device, fakes in tests).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/update/catalog.h"
#include "summit/update/image_check.h"
#include "summit/update/preflight.h"

namespace summit::update {

// Percent of the image written to flash (0..100), for the "Writing update" screen.
inline int write_percent(size_t done, size_t total)
{
    if (total == 0) return 0;
    if (done >= total) return 100;
    return static_cast<int>(done * 100 / total);
}

class OtaWriter {
public:
    virtual ~OtaWriter() = default;
    // Opens the spare app slot (never the running one, never factory).
    virtual bool begin(size_t image_size, std::string *error) = 0;
    virtual bool write(const uint8_t *data, size_t len, std::string *error) = 0;
    // Validates the written image (ESP-IDF checks its hash) and selects it for the next boot.
    virtual bool finish(std::string *error) = 0;
    virtual void abort() = 0;
};

class Hasher {
public:
    virtual ~Hasher() = default;
    virtual void reset() = 0;
    virtual void update(const uint8_t *data, size_t len) = 0;
    virtual std::string hex_digest() = 0;  // lowercase
};

enum class Stage {
    Idle,
    Checking,       // fetching the catalog
    UpToDate,
    Available,      // a newer build for this device + channel
    CheckFailed,
    Downloading,    // also used for web uploads
    Verifying,
    ReadyToReboot,  // image written and selected; reboot to run it (health gate follows)
    Failed,
};
const char *stage_name(Stage s);

enum class Source { None, Catalog, Upload };

struct Status {
    Stage stage = Stage::Idle;
    Source source = Source::None;
    std::string target_version;  // version being installed / available
    int64_t bytes = 0;
    int64_t total = 0;
    uint32_t preflight_failures = 0;
    std::string error;    // readable reason for CheckFailed / Failed
    int http_status = 0;  // last HTTP status (0 = none)
    std::string sha256;   // of the received image, once verified
};

class UpdateEngine {
public:
    UpdateEngine(DeviceInfo device, ImageExpect expect, OtaWriter &writer, Hasher &hasher);

    // -- catalog check
    bool begin_check();
    void catalog_received(const Catalog &catalog);
    void check_failed(const std::string &error, int http_status = 0);
    const std::vector<Candidate> &candidates() const { return candidates_; }
    const Candidate *available() const;  // newest installable update, if any
    // The entry to install for `version`: a catalog lists one entry per chip variant under the same version
    // (rev1 and rev3), so only the installable one for THIS device counts, whatever the order in the catalog.
    // nullptr if there is none.
    const Candidate *installable(const std::string &version) const;

    // -- install. Returns false (and sets Failed / keeps state) if busy or pre-flight fails.
    bool start_install(const std::string &version, PreflightInput preflight_in);
    bool start_upload(int64_t size, PreflightInput preflight_in);
    // Feed image bytes in order (download or upload). Returns false once failed.
    bool data(const uint8_t *p, size_t len);
    // All bytes received: verify size + SHA-256 (catalog), validate, select boot slot.
    bool complete();
    // Transport error (HTTP, TLS, socket, upload aborted).
    void fail(const std::string &error, int http_status = 0);
    void cancel();

    // The connection dropped mid-download: start the image over (aborts the writer, reopens it, clears the byte count,
    // hash and header check) without leaving the Downloading stage. False if not downloading or the slot cannot be reopened.
    bool restart_download();
    // Download finished: the stage becomes Verifying (shown as "Writing update") before complete() does the flash burst.
    bool begin_writing();
    bool busy() const { return status_.stage == Stage::Downloading || status_.stage == Stage::Verifying; }
    const Status &status() const { return status_; }
    const DeviceInfo &device() const { return device_; }
    void set_channel(Channel c) { device_.channel = c; }

private:
    bool start(Source source, int64_t size, const std::string &version, const std::string &sha256,
               PreflightInput preflight_in);
    void set_failed(const std::string &error);

    DeviceInfo device_;
    ImageExpect expect_;
    OtaWriter &writer_;
    Hasher &hasher_;
    Status status_;
    std::vector<Candidate> candidates_;
    int available_index_ = -1;
    std::string expected_sha256_;
    std::vector<uint8_t> header_;  // first bytes, checked before anything is written
    bool writer_open_ = false;
};

}  // namespace summit::update
