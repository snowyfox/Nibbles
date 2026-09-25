#include "nibbles_link.h"

#include <string.h>

void nl_chanscan_init(nl_chanscan_t *s, uint8_t start_channel)
{
    memset(s, 0, sizeof(*s));
    s->channel = (start_channel >= NL_CHANNEL_MIN && start_channel <= NL_CHANNEL_MAX) ? start_channel : NL_CHANNEL_MIN;
}

uint8_t nl_chanscan_tick(nl_chanscan_t *s, uint32_t dt_ms)
{
    s->on_channel_ms += dt_ms;
    s->since_heard_ms += dt_ms;
    if (s->locked) {
        if (s->since_heard_ms >= NL_LOCK_TIMEOUT_MS) {
            s->locked = false;  // the anchor went quiet: scan again, starting from here
            s->on_channel_ms = 0;
        }
    } else if (s->on_channel_ms >= NL_SCAN_DWELL_MS) {
        s->channel = s->channel >= NL_CHANNEL_MAX ? NL_CHANNEL_MIN : s->channel + 1;
        s->on_channel_ms = 0;
    }
    return s->channel;
}

void nl_chanscan_heard_anchor(nl_chanscan_t *s, uint8_t anchor_channel)
{
    if (anchor_channel >= NL_CHANNEL_MIN && anchor_channel <= NL_CHANNEL_MAX) s->channel = anchor_channel;
    if (!s->locked) s->locks++;
    s->locked = true;
    s->since_heard_ms = 0;
}
