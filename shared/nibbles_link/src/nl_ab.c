#include "nibbles_link.h"

#include <string.h>

void nl_ab_init(nl_ab_t *ab)
{
    memset(ab, 0, sizeof(*ab));
}

void nl_ab_frame(nl_ab_t *ab, int brain, uint32_t now_ms)
{
    ab->seen[brain & 1] = true;
    ab->frame_ms[brain & 1] = now_ms;
}

bool nl_ab_up(const nl_ab_t *ab, int brain, uint32_t now_ms)
{
    brain &= 1;
    return ab->seen[brain] && now_ms - ab->frame_ms[brain] < NL_AB_LOST_MS;
}

bool nl_ab_take(nl_ab_t *ab, uint32_t fade_ms, uint32_t now_ms)
{
    if (!nl_ab_up(ab, !ab->live, now_ms)) return false;
    const bool was_held = ab->hold;
    ab->live = !ab->live;
    ab->hold = false;
    ab->fading = !was_held && fade_ms > 0;
    ab->fade_start_ms = now_ms;
    ab->fade_ms = fade_ms;
    ab->takes++;
    return true;
}

bool nl_ab_take_to(nl_ab_t *ab, int brain, uint32_t fade_ms, uint32_t now_ms)
{
    if (ab->live == (brain & 1)) return true;
    return nl_ab_take(ab, fade_ms, now_ms);
}

void nl_ab_hold(nl_ab_t *ab, bool on, uint32_t now_ms)
{
    if (on && !nl_ab_up(ab, !ab->live, now_ms)) return;
    ab->hold = on;
    if (on) ab->fading = false;
}

bool nl_ab_tick(nl_ab_t *ab, uint32_t now_ms)
{
    bool changed = false;
    if (ab->fading && now_ms - ab->fade_start_ms >= ab->fade_ms) ab->fading = false;
    if (ab->hold && !nl_ab_up(ab, !ab->live, now_ms)) {
        ab->hold = false;
        changed = true;
    }
    if (!nl_ab_up(ab, ab->live, now_ms) && nl_ab_up(ab, !ab->live, now_ms)) {
        ab->live = !ab->live;
        ab->hold = ab->fading = false;
        ab->failovers++;
        changed = true;
    }
    return changed;
}

nl_ab_output_t nl_ab_output(const nl_ab_t *ab, uint32_t now_ms)
{
    nl_ab_output_t o = { .show = ab->hold ? !ab->live : ab->live, .from = 0, .weight = 255 };
    if (ab->fading && ab->fade_ms) {
        const uint32_t t = now_ms - ab->fade_start_ms;
        if (t < ab->fade_ms) {
            o.from = !ab->live;
            o.weight = (uint8_t)(t * 255 / ab->fade_ms);
        }
    }
    return o;
}

void nl_ab_state(const nl_ab_t *ab, uint32_t now_ms, const uint8_t fps[2], nl_ab_state_t *out)
{
    out->live = ab->live;
    out->showing = ab->hold ? !ab->live : ab->live;
    out->flags = NL_AB_ENABLED | (nl_ab_up(ab, 0, now_ms) ? NL_AB_A_UP : 0) | (nl_ab_up(ab, 1, now_ms) ? NL_AB_B_UP : 0) |
                 (ab->hold ? NL_AB_HOLDING : 0);
    out->fps[0] = fps[0];
    out->fps[1] = fps[1];
    out->takes = ab->takes;
    out->failovers = ab->failovers;
}

void nl_ab_mix(uint8_t *dst, const uint8_t *a, const uint8_t *b, size_t n, uint8_t weight)
{
    const unsigned w = weight, r = 255 - w;
    for (size_t i = 0; i < n; i++) dst[i] = (uint8_t)((a[i] * r + b[i] * w + 127) / 255);
}
