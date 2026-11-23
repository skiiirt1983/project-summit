// When the Wi-Fi service may try to join the saved network again. Pure logic, host-tested
// (tests/host/test_net_policy.cpp).
//
// Why it exists: the service used to call esp_wifi_disconnect() before every connection attempt. That disconnect raised
// a new "disconnected" event, which scheduled another attempt about 2 s later, and joining takes about 3 s: each
// retry cancelled the attempt before it, so the device never rejoined after the router came back (alpha.27: it brought
// up the Summit-Setup network and stayed there).
#pragma once
#include <cstdint>

namespace summit::net {

constexpr int kReasonAssocLeave = 8;           // WIFI_REASON_ASSOC_LEAVE: we disconnected on purpose
constexpr int64_t kFirstRetryMs = 3000;        // after a real drop
constexpr int64_t kRetryEveryMs = 8000;        // between attempts: longer than a join takes

class ReconnectPolicy {
public:
    // An attempt was just made (or new credentials were saved): the next one is not due for kRetryEveryMs.
    void attempted(int64_t now_ms) { next_ms_ = now_ms + kRetryEveryMs; }
    // The link dropped. Our own disconnect must not schedule another attempt.
    void disconnected(int64_t now_ms, int reason)
    {
        if (reason == kReasonAssocLeave) return;
        const int64_t soon = now_ms + kFirstRetryMs;
        if (next_ms_ > soon || next_ms_ < now_ms) next_ms_ = soon;  // let the radio settle, then retry; never sooner
    }
    bool due(int64_t now_ms, bool connected) const { return !connected && now_ms >= next_ms_; }

private:
    int64_t next_ms_ = 0;
};

}  // namespace summit::net
