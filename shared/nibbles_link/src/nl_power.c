#include "nibbles_link.h"

#include <string.h>

uint32_t nl_power_limit_ma(const nl_power_model_t *m, uint32_t pack_mv, int32_t current_ma, bool have_current)
{
    // Resting voltage: add back the sag across the pack while current flows.
    int64_t rest_mv = pack_mv;
    if (have_current && current_ma > 0) rest_mv += (int64_t)current_ma * m->pack_mohm / 1000;
    const int64_t loop = (int64_t)m->pack_mohm + m->wire_mohm;
    if (rest_mv <= m->floor_mv || loop <= 0) return m->min_ma;
    // mV * mV / mOhm = mW into the converter; times efficiency, over 5 V = mA out.
    const int64_t p_mw = (int64_t)m->floor_mv * (rest_mv - m->floor_mv) / loop;
    int64_t ma = p_mw * m->efficiency_pct / 100 / 5;
    if (ma < (int64_t)m->min_ma) ma = m->min_ma;
    if (ma > (int64_t)m->max_ma) ma = m->max_ma;
    return (uint32_t)ma;
}

void nl_power_limiter_init(nl_power_limiter_t *l, const nl_power_model_t *m)
{
    memset(l, 0, sizeof(*l));
    l->model = *m;
}

void nl_power_limiter_reading(nl_power_limiter_t *l, const nl_battery_t *b, uint32_t now_ms)
{
    if (b->pack_mv == 0) {
        const nl_power_model_t m = l->model;
        nl_power_limiter_init(l, &m);
        return;
    }
    l->target_ma = nl_power_limit_ma(&l->model, b->pack_mv, b->current_ma, (b->flags & NL_BATT_CURRENT) != 0);
    l->pack_mv = b->pack_mv;
    l->last_ms = now_ms;
    if (!l->heard || l->target_ma < l->limit_ma) l->limit_ma = l->target_ma;  // down at once
    if (!l->heard) l->tick_ms = now_ms;
    l->heard = true;
}

uint32_t nl_power_limiter_tick(nl_power_limiter_t *l, uint32_t now_ms)
{
    if (!l->heard) return 0;
    const uint32_t dt = now_ms - l->tick_ms;
    l->tick_ms = now_ms;
    if (l->limit_ma < l->target_ma) {
        const uint32_t step = (uint32_t)((uint64_t)dt * NL_POWER_RISE_MA_PER_S / 1000);
        l->limit_ma = l->target_ma - l->limit_ma > step ? l->limit_ma + step : l->target_ma;
    }
    return l->limit_ma;
}
