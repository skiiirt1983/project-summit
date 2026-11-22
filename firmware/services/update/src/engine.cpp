#include "summit/update/engine.h"

namespace summit::update {

const char *stage_name(Stage s)
{
    switch (s) {
    case Stage::Idle: return "idle";
    case Stage::Checking: return "checking";
    case Stage::UpToDate: return "up_to_date";
    case Stage::Available: return "available";
    case Stage::CheckFailed: return "check_failed";
    case Stage::Downloading: return "downloading";
    case Stage::Verifying: return "verifying";
    case Stage::ReadyToReboot: return "ready_to_reboot";
    case Stage::Failed: return "failed";
    }
    return "?";
}

UpdateEngine::UpdateEngine(DeviceInfo device, ImageExpect expect, OtaWriter &writer, Hasher &hasher)
    : device_(std::move(device)), expect_(std::move(expect)), writer_(writer), hasher_(hasher) {}

bool UpdateEngine::begin_check()
{
    if (busy() || status_.stage == Stage::ReadyToReboot) return false;
    status_ = Status{};
    status_.stage = Stage::Checking;
    return true;
}

void UpdateEngine::catalog_received(const Catalog &catalog)
{
    if (status_.stage != Stage::Checking) return;
    std::string error;
    if (!check_catalog(catalog, device_, &error)) {
        check_failed(error);
        return;
    }
    candidates_ = evaluate(catalog, device_);
    available_index_ = -1;
    Version current;
    if (!parse_version(device_.version, current)) current = Version{};
    const Candidate *best = newest_update(candidates_, current);
    if (best) {
        available_index_ = static_cast<int>(best - candidates_.data());
        status_.stage = Stage::Available;
        status_.target_version = best->build.version;
        status_.total = best->build.size;
    } else {
        status_.stage = Stage::UpToDate;
    }
}

void UpdateEngine::check_failed(const std::string &error, int http_status)
{
    if (status_.stage != Stage::Checking) return;
    status_.stage = Stage::CheckFailed;
    status_.error = error;
    status_.http_status = http_status;
}

const Candidate *UpdateEngine::available() const
{
    return available_index_ >= 0 ? &candidates_[available_index_] : nullptr;
}

const Candidate *UpdateEngine::installable(const std::string &version) const
{
    for (const Candidate &c : candidates_)
        if (c.build.version == version && c.reject == Reject::None) return &c;
    return nullptr;
}

bool UpdateEngine::start_install(const std::string &version, PreflightInput in)
{
    if (busy()) return false;
    if (const Candidate *c = installable(version)) {
        in.from_network = true;
        return start(Source::Catalog, c->build.size, version, c->build.sha256, in);
    }
    // Nothing installable: say why for the entry that matches best (the first one with this version).
    for (const Candidate &c : candidates_) {
        if (c.build.version != version) continue;
        set_failed(std::string("cannot install ") + version + ": " + reject_reason(c.reject));
        return false;
    }
    set_failed("version " + version + " is not in the catalog");
    return false;
}

bool UpdateEngine::start_upload(int64_t size, PreflightInput in)
{
    if (busy()) return false;
    in.from_network = false;
    return start(Source::Upload, size, "", "", in);
}

bool UpdateEngine::start(Source source, int64_t size, const std::string &version, const std::string &sha256,
                         PreflightInput in)
{
    in.image_size = size;
    in.slot_size = device_.slot_size;
    status_ = Status{};
    status_.source = source;
    status_.target_version = version;
    status_.total = size;
    status_.preflight_failures = preflight(in);
    if (status_.preflight_failures) {
        status_.stage = Stage::Failed;
        status_.error = preflight_message(status_.preflight_failures);
        return false;
    }
    std::string error;
    if (!writer_.begin(static_cast<size_t>(size), &error)) {
        set_failed("cannot open update slot: " + error);
        return false;
    }
    writer_open_ = true;
    expected_sha256_ = sha256;
    header_.clear();
    hasher_.reset();
    status_.stage = Stage::Downloading;
    return true;
}

bool UpdateEngine::data(const uint8_t *p, size_t len)
{
    if (status_.stage != Stage::Downloading) return false;
    if (status_.bytes + static_cast<int64_t>(len) > status_.total) {
        set_failed("received more data than expected");
        return false;
    }
    hasher_.update(p, len);
    status_.bytes += static_cast<int64_t>(len);
    std::string error;
    if (header_.size() < kImageHeaderBytes) {
        // Hold back the first bytes until the image header has been checked.
        header_.insert(header_.end(), p, p + len);
        if (header_.size() < kImageHeaderBytes && status_.bytes < status_.total) return true;
        ImageExpect e = expect_;
        e.version = status_.target_version;  // catalog: must match; upload: any
        std::string version;
        ImageError ie = check_image_header(header_.data(), header_.size(), e, &version);
        if (ie != ImageError::None) {
            set_failed(std::string("rejected image: ") + image_error_text(ie));
            return false;
        }
        if (status_.target_version.empty()) status_.target_version = version;
        if (!writer_.write(header_.data(), header_.size(), &error)) {
            set_failed("flash write failed: " + error);
            return false;
        }
        header_.resize(kImageHeaderBytes);  // keep marker that the header is done
        return true;
    }
    if (!writer_.write(p, len, &error)) {
        set_failed("flash write failed: " + error);
        return false;
    }
    return true;
}

bool UpdateEngine::begin_writing()
{
    if (status_.stage != Stage::Downloading || status_.bytes != status_.total) return false;
    status_.stage = Stage::Verifying;
    return true;
}

bool UpdateEngine::complete()
{
    if (status_.stage != Stage::Downloading && status_.stage != Stage::Verifying) return false;
    if (status_.bytes != status_.total) {
        set_failed("incomplete image: " + std::to_string(status_.bytes) + " of " + std::to_string(status_.total) +
                   " bytes");
        return false;
    }
    status_.stage = Stage::Verifying;
    status_.sha256 = hasher_.hex_digest();
    if (!expected_sha256_.empty() && status_.sha256 != expected_sha256_) {
        set_failed("SHA-256 does not match the catalog");
        return false;
    }
    std::string error;
    writer_open_ = false;
    if (!writer_.finish(&error)) {
        status_.stage = Stage::Failed;
        status_.error = "image validation failed: " + error;
        return false;
    }
    status_.stage = Stage::ReadyToReboot;
    return true;
}

void UpdateEngine::fail(const std::string &error, int http_status)
{
    if (!busy()) return;
    status_.http_status = http_status;
    set_failed(error);
}

void UpdateEngine::cancel()
{
    if (busy()) set_failed("cancelled");
}

void UpdateEngine::set_failed(const std::string &error)
{
    if (writer_open_) {
        writer_.abort();
        writer_open_ = false;
    }
    status_.stage = Stage::Failed;
    status_.error = error;
}

}  // namespace summit::update
