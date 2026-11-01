#include "power_sim.h"

#include <math.h>
#include <string.h>
#include "power_math.h"

#define PACK_R_MOHM     100     // as pack_percent assumes
#define EXT_WH          99      // a VB99
#define CHARGE_MA       1850    // the charger's share, and what it draws at the LEMO
#define CHARGE_IN_MA    2000
#define TAPER_FROM_PCT  90.0
#define PACK_LOW_END    25.0    // swap in an external battery here
#define EXT_END         8.0
#define SHOWS_A_NIGHT   3

int sim_rest_mv(double pct)
{
    int lo = 11000, hi = 17500;
    while (hi - lo > 2) {
        const int mid = (lo + hi) / 2;
        if (pack_percent(mid, 0) < pct) lo = mid;
        else hi = mid;
    }
    return hi;
}

void sim_reset(sim_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->pack_pct = 85;
    s->ext_pct = 100;
    s->cell_c = 24;
    s->die_c = 30;
    s->noise = 12345;
}

// Trance at 138 bpm through the lights: kicks, a slow swell, and a breakdown now and then.
static int show_ma(sim_state_t *s)
{
    s->noise = s->noise * 1664525u + 1013904223u;
    const double t = s->t_s, beat = fmod(t * 138 / 60, 1);
    const bool breakdown = fmod(t, 300) > 255;
    double ma = breakdown ? 1400 : 2300 + 1100 * exp(-5 * beat);
    ma += 400 * sin(t / 37);
    ma += (double)((s->noise >> 16) % 300) - 150;
    return (int)ma;
}

static void towards(double *v, double target, double dt_s, double tau_s)
{
    *v += (target - *v) * (1 - exp(-dt_s / tau_s));
}

void sim_realistic(sim_state_t *s, double dt_s, sim_reading_t *o)
{
    memset(o, 0, sizeof(*o));
    s->t_s += dt_s;
    o->charger_ok = o->efuse_pg = true;
    const bool hotel = s->phase == SIM_AT_HOTEL;
    o->light_ma = hotel ? 180 : show_ma(s);
    const int rest = sim_rest_mv(s->pack_pct);
    double charge_w = 0;

    if (s->phase == SIM_ON_PACK) {
        o->vbat_mv = rest - o->light_ma * PACK_R_MOHM / 1000;
        o->light_mv = o->vbat_mv - 150;
        o->chg_state = CHG_NOT_CHARGING;
        s->pack_pct -= (o->light_mv * o->light_ma / 1e6 + BASE_LOAD_W) * dt_s / 3600 / PACK_WH * 100;
        if (s->pack_pct <= PACK_LOW_END) {
            s->shows++;
            s->ext_pct = 100;
            s->phase = s->shows >= SHOWS_A_NIGHT ? SIM_AT_HOTEL : SIM_ON_EXTERNAL;
        }
    } else {
        o->docked = true;
        const double full = s->pack_pct >= 99.9 ? 0 : s->pack_pct < TAPER_FROM_PCT ? 1 : (100 - s->pack_pct) / (100 - TAPER_FROM_PCT);
        o->ibat_ma = (int)(CHARGE_MA * full);
        if (full > 0 && o->ibat_ma < 110) o->ibat_ma = 110;
        o->ibus_ma = o->ibat_ma ? o->ibat_ma * CHARGE_IN_MA / CHARGE_MA : 0;
        o->chg_state = !o->ibat_ma ? CHG_DONE : full < 1 ? CHG_TAPER : CHG_FAST;
        o->vbat_mv = rest + o->ibat_ma * PACK_R_MOHM / 1000;
        charge_w = o->vbat_mv * o->ibat_ma / 1e6;
        s->pack_pct += charge_w * dt_s / 3600 / PACK_WH * 100;
        if (s->pack_pct > 100) s->pack_pct = 100;
        if (hotel) {
            o->vbus_mv = 20100;
            o->light_mv = 19800;
            if (!o->ibat_ma) {             // charged: back out for the next night
                s->phase = SIM_ON_PACK;
                s->shows = 0;
            }
        } else {
            o->vbus_mv = sim_rest_mv(s->ext_pct);
            o->light_mv = o->vbus_mv - 200;
            const double w = o->vbus_mv * o->ibus_ma / 1e6 + o->light_mv * o->light_ma / 1e6 + BASE_LOAD_W;
            s->ext_pct -= w * dt_s / 3600 / EXT_WH * 100;
            if (s->ext_pct <= EXT_END) s->phase = SIM_ON_PACK;
        }
    }
    o->vsys_mv = o->vbat_mv + 200;
    o->light_mw = o->light_mv * o->light_ma / 1000;
    const double pack_a = (o->docked ? o->ibat_ma : o->light_ma) / 1000.0;
    towards(&s->cell_c, 24 + 1.5 * pack_a * pack_a, dt_s, 900);
    towards(&s->die_c, 30 + charge_w * 1.5, dt_s, 120);
    o->cell_c10 = (int)(s->cell_c * 10);
    o->die_c10 = (int)(s->die_c * 10);
}

// 0..1..0 over a period.
static double tri(double t, double period)
{
    const double x = fmod(t, period) / period;
    return x < 0.5 ? x * 2 : 2 - x * 2;
}

void sim_fast(double t, sim_reading_t *o)
{
    static const uint8_t states[] = { CHG_TRICKLE, CHG_PRECHARGE, CHG_FAST, CHG_TAPER, CHG_TOPOFF, CHG_DONE };
    memset(o, 0, sizeof(*o));
    const double input = fmod(t, 30), cycle = fmod(t, SIM_FAST_CYCLE_S);
    const double pct = 100 * tri(t, 40);
    o->charger_ok = o->efuse_pg = true;
    o->light_ma = (int)(8000 * tri(t, 7));
    o->die_c10 = 200 + (int)(1000 * tri(t, 23));     // through the screen's warn and bad levels
    o->cell_c10 = -80 + (int)(760 * tri(t, 17));
    o->docked = input >= 10;
    if (input < 10) {                                 // on the pack
        o->vbat_mv = sim_rest_mv(pct) - o->light_ma * PACK_R_MOHM / 1000;
        o->light_mv = o->vbat_mv - 150;
    } else {
        const bool supply = input >= 20;
        const bool starved = !supply && input < 14;   // plugged in, the pack gets almost nothing
        o->vbus_mv = supply ? 20100 : 13000 + (int)(3800 * tri(t, 10));
        o->ibat_ma = starved ? 30 : CHARGE_MA;
        o->ibus_ma = starved ? 60 : CHARGE_IN_MA;
        o->vbat_mv = sim_rest_mv(pct) + o->ibat_ma * PACK_R_MOHM / 1000;
        o->light_mv = o->vbus_mv - 200;
        o->chg_state = states[(int)(t / 2) % (int)sizeof(states)];
    }
    o->vsys_mv = o->vbat_mv + 200;
    o->light_mw = o->light_mv * o->light_ma / 1000;
    // Each problem the screen can report, for 4 s at the end of the cycle.
    if (cycle >= 100 && cycle < 104) o->fault0 = 0x20;
    else if (cycle >= 104 && cycle < 108) o->efuse_pg = false;
    else if (cycle >= 108 && cycle < 112) o->ts_flags = 0x01;
    else if (cycle >= 112 && cycle < 116) o->ts_flags = 0x08;
    else if (cycle >= 116) o->charger_ok = false;
}
