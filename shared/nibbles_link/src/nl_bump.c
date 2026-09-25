#include "nibbles_link.h"

#include <string.h>

void nl_bump_rx_init(nl_bump_rx_t *rx)
{
    memset(rx, 0, sizeof(*rx));
}

nl_bump_event_t nl_bump_rx_message(nl_bump_rx_t *rx, const nl_bump_t *b, uint32_t now_ms)
{
    const bool same = rx->active && rx->bump.id == b->id;
    if (b->phase == NL_BUMP_STOP) {
        if (!same) return NL_BUMP_EV_NONE;  // a stop for something already over
        rx->active = false;
        return NL_BUMP_EV_END;
    }
    if (b->phase != NL_BUMP_START && b->phase != NL_BUMP_HOLD) return NL_BUMP_EV_NONE;
    if (same) {
        rx->last_ms = now_ms;  // keepalive (or a repeated start)
        return NL_BUMP_EV_NONE;
    }
    // A new press: START, or HOLD when the START was lost.
    const bool was_active = rx->active;
    rx->bump = *b;
    rx->active = true;
    rx->last_ms = now_ms;
    return was_active ? NL_BUMP_EV_REPLACE : NL_BUMP_EV_BEGIN;
}

nl_bump_event_t nl_bump_rx_tick(nl_bump_rx_t *rx, uint32_t now_ms)
{
    if (rx->active && (uint32_t)(now_ms - rx->last_ms) >= NL_BUMP_TIMEOUT_MS) {
        rx->active = false;
        return NL_BUMP_EV_END;
    }
    return NL_BUMP_EV_NONE;
}
