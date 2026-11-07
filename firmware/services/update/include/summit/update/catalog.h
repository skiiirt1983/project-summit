// Update catalog (format 1): which builds exist, and which this device may install.
// The device parses catalog.json with cJSON into these structs (catalog_json.cpp);
// everything here is pure logic, host-tested. Format: docs/ARCHITECTURE.md "Update catalog".
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/update/version.h"

namespace summit::update {

constexpr int64_t kCatalogFormat = 1;
constexpr int64_t kPartitionLayout = 1;  // bump only with a new partitions.csv (never, per hard rule 1)

enum class Channel { Stable, Beta, Alpha };
bool parse_channel(const std::string &s, Channel &out);
const char *channel_name(Channel c);

struct CatalogBuild {
    std::string version;
    std::string channel;      // "stable" | "beta" | "alpha"
    std::string hardware_id;  // board + chip-revision family, e.g. waveshare-p4-wifi6-lcd5-rev3
    int64_t partition_layout = 0;
    std::string url;          // https, host on the allow-list
    int64_t size = 0;         // bytes of the OTA (app-only) image
    std::string sha256;       // 64 lowercase hex
    std::string released;     // YYYY-MM-DD (display only)
    std::string notes;        // short changelog (display only)
};

struct Catalog {
    int64_t format = 0;
    std::string project;
    std::vector<CatalogBuild> builds;
};

struct DeviceInfo {
    std::string project = "summit";
    std::string hardware_id;
    std::string version;  // running app version
    int64_t partition_layout = kPartitionLayout;
    int64_t slot_size = 0;
    Channel channel = Channel::Stable;
    std::vector<std::string> allowed_hosts;
};

enum class Reject {
    None,
    BadVersion,
    BadChannel,
    ChannelNotFollowed,  // e.g. a beta build while following stable
    WrongHardware,       // other board or other chip-revision family: never installable
    WrongLayout,
    BadUrl,
    HostNotAllowed,
    BadSize,
    BadSha256,
};
const char *reject_reason(Reject r);

struct Candidate {
    CatalogBuild build;
    Version version;
    Reject reject = Reject::None;
};

// Format and project must match; otherwise the whole catalog is ignored.
bool check_catalog(const Catalog &catalog, const DeviceInfo &device, std::string *error);
// Every build with the reason it can't be installed (Reject::None = installable).
std::vector<Candidate> evaluate(const Catalog &catalog, const DeviceInfo &device);
// Newest installable build strictly newer than `current`; nullptr if none.
const Candidate *newest_update(const std::vector<Candidate> &candidates, const Version &current);
// Host part of an https URL ("" if not a plain https://host/path URL).
std::string url_host(const std::string &url);

}  // namespace summit::update
