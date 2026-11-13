// HTTPS page fetcher for recipe URL import (device only). UNTESTED ON HARDWARE:
// see docs/HARDWARE_TODO.md.
#pragma once
#include "summit/recipes/jsonld.h"

namespace summit::recipes {

// Certificate bundle, up to 3 redirects, 15 s timeout, body capped at max_bytes.
class HttpsPageFetcher : public PageFetcher {
public:
    bool get(const std::string &url, size_t max_bytes, std::string &body, std::string *error) override;
};

}  // namespace summit::recipes
