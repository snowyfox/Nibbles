#include "nibbles_link.h"

#include <string.h>

void nl_chanscan_init(nl_chanscan_t *s, uint8_t start_channel)
{
    memset(s, 0, sizeof(*s));
    s->channel = (start_channel >= NL_CHANNEL_MIN && start_channel <= NL_CHANNEL_MAX) ? start_channel : NL_CHANNEL_MIN;
    s->home_channel = s->peek_channel = s->channel;
}

void nl_chanscan_set_fallback(nl_chanscan_t *s, uint32_t fallback_ms)
{
    s->fallback_ms = fallback_ms;
}

static uint8_t next_channel(uint8_t ch)
{
    return ch >= NL_CHANNEL_MAX ? NL_CHANNEL_MIN : ch + 1;
}

static uint8_t anchoring_tick(nl_chanscan_t *s)
{
    if (s->peeking) {
        if (s->on_channel_ms >= NL_PEEK_MS) {
            s->peeking = false;
            s->channel = s->home_channel;
            s->on_channel_ms = 0;
        }
    } else if (s->on_channel_ms >= NL_PEEK_EVERY_MS) {
        s->peek_channel = next_channel(s->peek_channel);
        if (s->peek_channel == s->home_channel) s->peek_channel = next_channel(s->peek_channel);
        s->peeking = true;
        s->channel = s->peek_channel;
        s->on_channel_ms = 0;
    }
    return s->channel;
}

uint8_t nl_chanscan_tick(nl_chanscan_t *s, uint32_t dt_ms)
{
    s->on_channel_ms += dt_ms;
    s->since_heard_ms += dt_ms;
    if (s->anchoring) return anchoring_tick(s);
    if (s->fallback_ms && !s->locked && s->since_heard_ms >= s->fallback_ms) {
        s->anchoring = true;  // nobody else is anchoring: do it ourselves
        s->anchorings++;
        s->peeking = false;
        s->channel = s->peek_channel = s->home_channel;
        s->on_channel_ms = 0;
        return s->channel;
    }
    if (s->locked) {
        if (s->since_heard_ms >= NL_LOCK_TIMEOUT_MS) {
            s->locked = false;  // the anchor went quiet: scan again, starting from here
            s->on_channel_ms = 0;
        }
    } else if (s->on_channel_ms >= NL_SCAN_DWELL_MS) {
        s->channel = next_channel(s->channel);
        s->on_channel_ms = 0;
    }
    return s->channel;
}

void nl_chanscan_heard_anchor(nl_chanscan_t *s, uint8_t anchor_channel)
{
    s->anchoring = s->peeking = false;
    s->on_channel_ms = 0;
    if (anchor_channel >= NL_CHANNEL_MIN && anchor_channel <= NL_CHANNEL_MAX) s->channel = anchor_channel;
    if (!s->locked) s->locks++;
    s->locked = true;
    s->since_heard_ms = 0;
}
