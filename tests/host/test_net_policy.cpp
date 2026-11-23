#include "check.h"
#include "summit/net/reconnect.h"

using summit::net::ReconnectPolicy;
using namespace summit::net;

TEST(first_attempt_is_due_at_once_then_paced)
{
    ReconnectPolicy p;
    CHECK(p.due(0, false));
    p.attempted(0);
    CHECK(!p.due(1000, false));                 // a join takes about 3 s: do not cancel it
    CHECK(!p.due(kRetryEveryMs - 1, false));
    CHECK(p.due(kRetryEveryMs, false));
    CHECK(!p.due(kRetryEveryMs, true));         // connected: nothing to retry
}

TEST(our_own_disconnect_does_not_schedule_a_retry)
{
    // The bug: disconnect before connect raised a disconnected event (reason 8) that scheduled the next attempt.
    ReconnectPolicy p;
    p.attempted(10000);                          // attempt in progress
    p.disconnected(10500, kReasonAssocLeave);    // the event our esp_wifi_disconnect() caused
    CHECK(!p.due(13500, false));                 // still not due: the attempt is left alone
    CHECK(p.due(10000 + kRetryEveryMs, false));
}

TEST(a_real_drop_retries_soon_but_never_faster_than_a_join)
{
    ReconnectPolicy p;
    p.attempted(0);
    p.disconnected(60000, 200);                  // beacon timeout (router off)
    CHECK(!p.due(60000 + kFirstRetryMs - 1, false));
    CHECK(p.due(60000 + kFirstRetryMs, false));
    p.attempted(63000);                          // that attempt fails
    p.disconnected(64000, 201);                  // NO_AP_FOUND: the attempt is over, router still off
    CHECK(!p.due(64000 + kFirstRetryMs - 1, false));
    CHECK(p.due(64000 + kFirstRetryMs, false));  // the failed attempt is over, so the next one may start
}

TEST(router_returns_after_a_long_outage)
{
    // 5 minutes of failed attempts every 8 s, then the router is back: the next attempt joins (nothing cancels it).
    ReconnectPolicy p;
    int64_t now = 0;
    int attempts = 0;
    for (; now < 300000; now += 1000) {
        if (p.due(now, false)) {
            p.attempted(now);
            ++attempts;
        }
    }
    CHECK(attempts >= 30 && attempts <= 40);     // about one attempt per 8 s, not one per 2 s
    p.attempted(now);                            // the join starts here and takes 3 s
    CHECK(!p.due(now + 3000, false));            // it is not interrupted
    CHECK(!p.due(now + 5000, true));             // joined
}

int main()
{
    RUN(first_attempt_is_due_at_once_then_paced);
    RUN(our_own_disconnect_does_not_schedule_a_retry);
    RUN(a_real_drop_retries_soon_but_never_faster_than_a_join);
    RUN(router_returns_after_a_long_outage);
    return report("net policy");
}
