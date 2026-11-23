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
constexpr int64_t kFirstRetryMs = 3000;        // after a real drop of a working connection
constexpr int64_t kRetryEveryMs = 8000;        // between attempts: longer than a join takes
// While the setup network (Summit-Setup) is up the chip has one radio for both: every join attempt takes it off the setup
// network's channel for several seconds, so a phone cannot join it (alpha.31 log: no phone ever connected in 3 minutes,
// and the router handshake kept timing out). Try rarely then, and more rarely still while a phone is connected to it.
constexpr int64_t kRetryWhileSetupApMs = 60000;
constexpr int64_t kRetryWhileSetupClientMs = 90000;

constexpr int64_t kDhcpWaitMs = 20000;  // associated but no address yet: leave the join alone for this long

// True while the station is associated with the router and waiting for its address (DHCP). A retry or a scan in this
// window cancels or slows the address (alpha.30: 36 s to get an address instead of about 1 s). After kDhcpWaitMs it is
// treated as a failed join again.
inline bool associating(bool associated, bool connected, int64_t now_ms, int64_t associated_since_ms)
{
    return associated && !connected && now_ms - associated_since_ms < kDhcpWaitMs;
}

class ReconnectPolicy {
public:
    // An attempt was just made (or new credentials were saved): the next one is not due for `interval_ms`.
    void attempted(int64_t now_ms, int64_t interval_ms = kRetryEveryMs) { next_ms_ = now_ms + interval_ms; }
    // The link dropped. Our own disconnect (reason 8) schedules nothing. A *failed attempt* (the router is still off) also
    // schedules nothing: only the drop of a working connection (real_drop) brings the next attempt forward.
    void disconnected(int64_t now_ms, int reason, bool real_drop = true)
    {
        if (reason == kReasonAssocLeave || !real_drop) return;
        const int64_t soon = now_ms + kFirstRetryMs;
        if (next_ms_ > soon || next_ms_ < now_ms) next_ms_ = soon;  // let the radio settle, then retry; never sooner
    }
    bool due(int64_t now_ms, bool connected) const { return !connected && now_ms >= next_ms_; }

private:
    int64_t next_ms_ = 0;
};

// How long to wait between join attempts, for the current state.
inline int64_t retry_interval_ms(bool setup_ap_on, int ap_clients)
{
    if (!setup_ap_on) return kRetryEveryMs;
    return ap_clients > 0 ? kRetryWhileSetupClientMs : kRetryWhileSetupApMs;
}

}  // namespace summit::net
