#include "nibbles_link.h"

#include <string.h>

void nl_chanscan_init(nl_chanscan_t *s, uint8_t start_channel)
{
    memset(s, 0, sizeof(*s));
    s->channel = (start_channel >= NL_CHANNEL_MIN && start_channel <= NL_CHANNEL_MAX) ? start_channel : NL_CHANNEL_MIN;
    s->home_channel = s->channel;
}

void nl_chanscan_set_fallback(nl_chanscan_t *s, uint32_t fallback_ms)
{
    s->fallback_ms = fallback_ms;
}

void nl_chanscan_set_self(nl_chanscan_t *s, const uint8_t mac[6])
{
    memcpy(s->self, mac, 6);
}

static uint8_t next_channel(uint8_t ch)
{
    return ch >= NL_CHANNEL_MAX ? NL_CHANNEL_MIN : ch + 1;
}

static bool valid_channel(uint8_t ch)
{
    return ch >= NL_CHANNEL_MIN && ch <= NL_CHANNEL_MAX;
}


static void move_to(nl_chanscan_t *s, uint8_t ch)
{
    s->channel = ch;
    s->on_channel_ms = 0;
}

static void schedule(nl_chanscan_t *s, uint8_t kind, uint8_t ch, int32_t in_ms)
{
    s->hop_pending = true;
    s->hop_kind = kind;
    s->hop_channel = ch;
    s->hop_in_ms = in_ms;
}

uint8_t nl_chanscan_tick(nl_chanscan_t *s, uint32_t dt_ms)
{
    s->on_channel_ms += dt_ms;
    s->since_heard_ms += dt_ms;
    s->move_hold_ms = s->move_hold_ms > dt_ms ? s->move_hold_ms - dt_ms : 0;
    if (s->hop_pending) {
        s->hop_in_ms -= (int32_t)dt_ms;
        if (s->hop_in_ms <= 0) {
            s->hop_pending = false;
            move_to(s, s->hop_channel);
            if (s->locked) s->since_heard_ms = 0;   // the anchor or leader is coming here too
            if (s->hop_kind == NL_HOP_MOVE) {
                s->grace_ms = NL_MOVE_GRACE_MS;
                s->move_hold_ms = NL_MOVE_HOLD_MS;
            }
        }
        return s->channel;
    }
    if (s->leading) {
        // Announce the next channel and move NL_HOP_NOTICE_MS later, the same
        // countdown the followers run. The dwell varies a little with the MAC,
        // so two leaders sweeping apart meet in the end.
        const uint32_t dwell = NL_GROUP_DWELL_MS + 50u * (s->self[5] % 5u);
        if (s->on_channel_ms + NL_HOP_NOTICE_MS >= dwell) {
            s->announce = true;
            s->announce_channel = next_channel(s->channel);
            schedule(s, NL_HOP_SEARCH, s->announce_channel, NL_HOP_NOTICE_MS);
        }
        return s->channel;
    }
    if (s->locked) {
        if (s->since_heard_ms >= NL_LOCK_TIMEOUT_MS + s->grace_ms) {
            s->locked = s->group = false;   // the anchor (or leader) went quiet: scan again, starting from here
            s->grace_ms = 0;
            s->on_channel_ms = 0;
        }
        return s->channel;
    }
    if (s->fallback_ms && s->since_heard_ms >= s->fallback_ms) {
        s->leading = true;   // nobody to follow: lead a search ourselves
        s->leads++;
        move_to(s, s->home_channel);
        return s->channel;
    }
    if (s->on_channel_ms >= NL_SCAN_DWELL_MS) move_to(s, next_channel(s->channel));
    return s->channel;
}

void nl_chanscan_heard_anchor(nl_chanscan_t *s, uint8_t anchor_channel)
{
    if (s->hop_pending && s->hop_kind == NL_HOP_MOVE) {
        s->since_heard_ms = 0;   // still on the old channel until the move: keep the lock
        return;
    }
    if (s->move_hold_ms && valid_channel(anchor_channel) && anchor_channel != s->channel)
        return;   // just moved: an anchor still advertising another channel is one that hasn't moved yet
    s->leading = s->group = false;
    s->hop_pending = false;
    if (valid_channel(anchor_channel) && anchor_channel != s->channel) move_to(s, anchor_channel);
    if (!s->locked) s->locks++;
    s->locked = true;
    s->since_heard_ms = 0;
    s->grace_ms = 0;
}

static void follow(nl_chanscan_t *s, const uint8_t mac[6])
{
    s->leading = false;
    memcpy(s->leader, mac, 6);
    if (!s->locked) s->locks++;
    s->locked = s->group = true;
    s->since_heard_ms = 0;
}

// Whether a search leader at mac is one to follow now.
static bool take_leader(nl_chanscan_t *s, const uint8_t mac[6])
{
    if (s->locked && !s->group) return false;               // an anchor beats any search
    if (s->leading) return memcmp(mac, s->self, 6) < 0;     // the lower MAC leads
    if (s->locked && s->group && memcmp(mac, s->leader, 6) != 0)
        return memcmp(mac, s->leader, 6) < 0 || s->since_heard_ms >= 1000;   // a lower one, or ours went quiet
    return true;
}

void nl_chanscan_heard_group(nl_chanscan_t *s, const uint8_t mac[6], uint8_t channel)
{
    if (!take_leader(s, mac)) return;
    follow(s, mac);
    if (valid_channel(channel) && channel != s->channel && !s->hop_pending) move_to(s, channel);
}

void nl_chanscan_heard_hop(nl_chanscan_t *s, const uint8_t mac[6], const nl_hop_t *hop)
{
    if (!valid_channel(hop->channel)) return;
    if (hop->kind == NL_HOP_SEARCH) {
        if (!take_leader(s, mac)) return;
        follow(s, mac);
        schedule(s, NL_HOP_SEARCH, hop->channel, hop->in_ms);
    } else if (hop->kind == NL_HOP_MOVE) {
        nl_chanscan_move(s, hop->channel, hop->in_ms);
    }
}

void nl_chanscan_move(nl_chanscan_t *s, uint8_t channel, uint32_t in_ms)
{
    // Only a network that has an anchor moves; a search keeps searching.
    if (!s->locked || s->group || !valid_channel(channel)) return;
    if (channel == s->channel && !s->hop_pending) return;
    schedule(s, NL_HOP_MOVE, channel, (int32_t)in_ms);
    s->since_heard_ms = 0;
}

uint8_t nl_chanscan_busy(nl_chanscan_t *s)
{
    return s->channel;
}

// ---------------------------------------------------------------- site survey

// How much of a channel's neighbour, k channels away, reaches it (percent):
// 20 MHz wide channels 5 MHz apart, with the transmit mask's fall-off.
static const uint8_t overlap_pct[5] = { 100, 70, 40, 15, 5 };

void nl_survey_score(nl_survey_t *s, uint8_t max_channel)
{
    if (max_channel < NL_CHANNEL_MIN || max_channel > NL_CHANNEL_MAX) max_channel = NL_CHANNEL_MAX;
    for (int c = NL_CHANNEL_MIN; c <= NL_CHANNEL_MAX; c++) {
        uint32_t sum = 0;
        for (int k = NL_CHANNEL_MIN; k <= NL_CHANNEL_MAX; k++) {
            const int d = c > k ? c - k : k - c;
            if (d > 4) continue;
            // Airtime measured, plus what the access points nearby will bring (half a
            // per mille per dB above -95 dBm).
            sum += overlap_pct[d] * (s->ch[k].busy_pml + s->ch[k].ap_weight / 2u);
        }
        s->ch[c].load = (uint16_t)(sum / 100 > 65535 ? 65535 : sum / 100);
    }
    uint8_t best = NL_CHANNEL_MIN;
    for (int c = NL_CHANNEL_MIN; c <= max_channel; c++)
        if (s->ch[c].load < s->ch[best].load) best = (uint8_t)c;
    const uint8_t cur = s->current;
    if (cur >= NL_CHANNEL_MIN && cur <= max_channel) {
        // Worth a move only if clearly better: a fifth less, and by more than 2% of airtime.
        const uint32_t lc = s->ch[cur].load, lb = s->ch[best].load;
        s->worth_moving = best != cur && lb * 5 < lc * 4 && lc - lb > 20;
        if (!s->worth_moving) best = cur;
    } else {
        s->worth_moving = true;   // off the allowed channels: any of them is better
    }
    s->best = best;
}
