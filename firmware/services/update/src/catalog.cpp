#include "summit/update/catalog.h"

#include <algorithm>

namespace summit::update {

bool parse_channel(const std::string &s, Channel &out)
{
    if (s == "stable") out = Channel::Stable;
    else if (s == "beta") out = Channel::Beta;
    else if (s == "alpha") out = Channel::Alpha;
    else return false;
    return true;
}

const char *channel_name(Channel c)
{
    switch (c) {
    case Channel::Stable: return "stable";
    case Channel::Beta: return "beta";
    case Channel::Alpha: return "alpha";
    }
    return "?";
}

const char *reject_reason(Reject r)
{
    switch (r) {
    case Reject::None: return "installable";
    case Reject::BadVersion: return "invalid version";
    case Reject::BadChannel: return "invalid channel";
    case Reject::ChannelNotFollowed: return "channel not followed";
    case Reject::WrongHardware: return "built for different hardware";
    case Reject::WrongLayout: return "different partition layout";
    case Reject::BadUrl: return "invalid URL";
    case Reject::HostNotAllowed: return "download host not allowed";
    case Reject::BadSize: return "invalid size";
    case Reject::BadSha256: return "invalid SHA-256";
    }
    return "?";
}

std::string url_host(const std::string &url)
{
    const std::string scheme = "https://";
    if (url.compare(0, scheme.size(), scheme) != 0) return "";
    size_t end = url.find('/', scheme.size());
    if (end == std::string::npos || end == scheme.size() || end + 1 >= url.size()) return "";
    std::string host = url.substr(scheme.size(), end - scheme.size());
    for (char c : host) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (!ok) return "";  // no credentials, ports, upper case or odd characters
    }
    for (char c : url)
        if (c <= ' ' || c == '\\' || c == '"' || c == 0x7F) return "";
    return host;
}

bool check_catalog(const Catalog &catalog, const DeviceInfo &device, std::string *error)
{
    if (catalog.format != kCatalogFormat) {
        if (error) *error = "unsupported catalog format " + std::to_string(catalog.format);
        return false;
    }
    if (catalog.project != device.project) {
        if (error) *error = "catalog is for project '" + catalog.project + "'";
        return false;
    }
    return true;
}

static bool hex64(const std::string &s)
{
    if (s.size() != 64) return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

static bool follows(Channel device, Channel build)
{
    // stable sees stable; beta sees stable + beta; alpha sees everything.
    return static_cast<int>(build) <= static_cast<int>(device);
}

std::vector<Candidate> evaluate(const Catalog &catalog, const DeviceInfo &device)
{
    std::vector<Candidate> out;
    for (const CatalogBuild &b : catalog.builds) {
        Candidate c;
        c.build = b;
        Channel ch;
        if (!parse_version(b.version, c.version)) c.reject = Reject::BadVersion;
        else if (!parse_channel(b.channel, ch)) c.reject = Reject::BadChannel;
        else if (b.hardware_id != device.hardware_id) c.reject = Reject::WrongHardware;
        else if (b.partition_layout != device.partition_layout) c.reject = Reject::WrongLayout;
        else if (url_host(b.url).empty()) c.reject = Reject::BadUrl;
        else if (std::find(device.allowed_hosts.begin(), device.allowed_hosts.end(), url_host(b.url)) ==
                 device.allowed_hosts.end())
            c.reject = Reject::HostNotAllowed;
        else if (b.size <= 0 || b.size > device.slot_size) c.reject = Reject::BadSize;
        else if (!hex64(b.sha256)) c.reject = Reject::BadSha256;
        else if (!follows(device.channel, ch)) c.reject = Reject::ChannelNotFollowed;
        out.push_back(c);
    }
    return out;
}

const Candidate *newest_update(const std::vector<Candidate> &candidates, const Version &current)
{
    const Candidate *best = nullptr;
    for (const Candidate &c : candidates) {
        if (c.reject != Reject::None || compare_versions(c.version, current) <= 0) continue;
        if (!best || compare_versions(c.version, best->version) > 0) best = &c;
    }
    return best;
}

}  // namespace summit::update
