// Firmware versions: MAJOR.MINOR.PATCH with an optional -prerelease (SemVer 2.0
// precedence, build metadata not allowed). Pure logic.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace summit::update {

struct Version {
    uint32_t major = 0, minor = 0, patch = 0;
    std::vector<std::string> pre;  // dot-separated prerelease identifiers; empty = release
};

bool parse_version(const std::string &text, Version &out);
// <0, 0, >0. Prerelease sorts before its release: 0.2.0-beta.1 < 0.2.0.
int compare_versions(const Version &a, const Version &b);

}  // namespace summit::update
