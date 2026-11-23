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

TEST(an_association_waiting_for_its_address_is_left_alone)
{
    CHECK(!associating(false, false, 5000, 0));                 // not associated: nothing to protect
    CHECK(associating(true, false, 1000, 0));                   // associated, no address yet: no retries, no scans
    CHECK(associating(true, false, kDhcpWaitMs - 1, 0));
    CHECK(!associating(true, false, kDhcpWaitMs, 0));           // too long: a failed join, retry
    CHECK(!associating(true, true, 1000, 0));                   // got its address: connected
    // the alpha.30 log: associated at 219.5 s, retries at 227, 235, 243 and 251 s cancelled the address until 255 s
    ReconnectPolicy p;
    p.attempted(216000);
    const int64_t assoc = 219563;
    for (int64_t t = 220000; t < 239000; t += 1000) CHECK(!p.due(t, false || associating(true, false, t, assoc)));
}

TEST(failed_attempts_do_not_pull_the_next_attempt_forward_and_the_setup_network_slows_them)
{
    // alpha.30/31 logs: attempt, "disconnected (reason 201)" 2.4 s later, next attempt 4 s after that: every 6.4 s forever.
    ReconnectPolicy p;
    p.attempted(100000);                                           // an attempt; the router is off
    p.disconnected(102400, 201, false);                            // it failed: not a drop of a working link
    CHECK(!p.due(106400, false));                                  // the old behaviour retried here
    CHECK(p.due(100000 + kRetryEveryMs, false));
    // a real drop of a working connection still retries soon
    ReconnectPolicy q;
    q.attempted(0);
    q.disconnected(60000, 200, true);
    CHECK(q.due(60000 + kFirstRetryMs, false));
    // with the setup network up: one attempt a minute, 90 s while a phone is connected to it
    CHECK(retry_interval_ms(false, 0) == kRetryEveryMs);
    CHECK(retry_interval_ms(true, 0) == 60000 && retry_interval_ms(true, 1) == 90000 && retry_interval_ms(true, 3) == 90000);
    ReconnectPolicy r;
    r.attempted(500000, retry_interval_ms(true, 0));
    CHECK(!r.due(559999, false) && r.due(560000, false));
    r.attempted(560000, retry_interval_ms(true, 1));
    CHECK(!r.due(649999, false) && r.due(650000, false));
}

int main()
{
    RUN(first_attempt_is_due_at_once_then_paced);
    RUN(our_own_disconnect_does_not_schedule_a_retry);
    RUN(a_real_drop_retries_soon_but_never_faster_than_a_join);
    RUN(router_returns_after_a_long_outage);
    RUN(an_association_waiting_for_its_address_is_left_alone);
    RUN(failed_attempts_do_not_pull_the_next_attempt_forward_and_the_setup_network_slows_them);
    return report("net policy");
}
