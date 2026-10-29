#include "power_math.h"

#include <math.h>

// Resting voltage per cell (mV) against state of charge, for a high-energy
// 21700 at low rates; rough, but steady enough for a bar and a runtime guess.
static const struct { int mv, pct; } curve[] = {
    { 3000, 0 }, { 3300, 3 }, { 3450, 8 }, { 3550, 15 }, { 3620, 25 }, { 3680, 35 }, { 3740, 45 },
    { 3800, 55 }, { 3870, 65 }, { 3950, 75 }, { 4040, 85 }, { 4120, 93 }, { 4180, 100 },
};

int pack_percent(int pack_mv, int current_ma)
{
    // The voltage the pack would rest at: add back the sag across its resistance.
    const int rest = pack_mv + (current_ma > 0 ? current_ma * PACK_MOHM / 1000 : 0);
    const int cell = rest / PACK_CELLS;
    const int n = sizeof(curve) / sizeof(curve[0]);
    if (cell <= curve[0].mv) return 0;
    if (cell >= curve[n - 1].mv) return 100;
    for (int i = 1; i < n; i++) {
        if (cell <= curve[i].mv) {
            const int a = curve[i - 1].mv, b = curve[i].mv;
            return curve[i - 1].pct + (curve[i].pct - curve[i - 1].pct) * (cell - a) / (b - a);
        }
    }
    return 100;
}

input_kind_t input_kind(int vbus_mv)
{
    if (vbus_mv < INPUT_PRESENT_MV) return INPUT_NONE;
    return vbus_mv > INPUT_SUPPLY_MV ? INPUT_SUPPLY : INPUT_BATTERY;
}

power_flow_t power_flow(input_kind_t input, int vbus_mv, int ibus_ma, int pack_mv, int ibat_ma,
                        int lights_ma, int base_mw, bool charge_done)
{
    power_flow_t f = { 0, -1, ibat_ma, false };
    const int bus_mv = input != INPUT_NONE ? vbus_mv : pack_mv;
    const int load_ma = (lights_ma > 0 ? lights_ma : 0) + (bus_mv > 0 ? base_mw * 1000 / bus_mv : 0);
    const bool lights_from_input = input != INPUT_NONE && vbus_mv > pack_mv - BODY_DIODE_MV;
    if (input == INPUT_NONE || !lights_from_input) f.pack_ma -= load_ma;   // the pack feeds the light bus
    if (input == INPUT_NONE) return f;
    const int to_charger = ibus_ma > 0 ? ibus_ma : 0;
    f.in_ma = to_charger + (lights_from_input ? load_ma : 0);
    f.split_permille = f.in_ma > 0 ? to_charger * 1000 / f.in_ma : 0;
    f.starved = !charge_done && (ibat_ma < STARVED_MA || !lights_from_input);
    return f;
}

int runtime_minutes(int pct, int load_mw)
{
    if (load_mw < 500) return -1;
    return (int)((int64_t)pct * PACK_WH * 60 * 1000 / 100 / load_mw);
}

uint16_t ina226_calibration(void)
{
    // CAL = 0.00512 / (current LSB x R shunt), with the LSB in A and R in ohm.
    return (uint16_t)(5120000000.0 / ((double)INA226_CURRENT_LSB_UA * INA226_SHUNT_MOHM * 1000.0) + 0.5);
}

int ina226_bus_mv(uint16_t reg) { return reg * 125 / 100; }
int ina226_current_ma(uint16_t reg) { return (int16_t)reg * INA226_CURRENT_LSB_UA / 1000; }
int ina226_power_mw(uint16_t reg) { return reg * 25 * INA226_CURRENT_LSB_UA / 1000; }

chg_state_t bq_charge_state(uint8_t reg1c) { return (chg_state_t)(reg1c >> 5 & 7); }

const char *bq_charge_state_name(chg_state_t s)
{
    static const char *names[] = { "not charging", "trickle", "pre-charge", "fast charge", "taper",
                                   "?", "top-off", "full" };
    return s <= CHG_DONE ? names[s] : "?";
}

int bq_ts_tenths_pct(uint16_t reg3f) { return (int)(reg3f * 0.0976563 * 10 + 0.5); }
int bq_die_tenths_c(uint16_t reg41) { return (int16_t)reg41 * 5; }

int ts_cell_tenths_c(int ts_tenths_pct)
{
    // TS = REGN x (R_low || NTC) / (R_high + (R_low || NTC)), R_high 5.23k, R_low 30.1k.
    const double f = ts_tenths_pct / 1000.0;
    if (f <= 0.05 || f >= 0.95) return INT16_MIN;
    const double par = 5230.0 * f / (1.0 - f);       // R_low || NTC
    if (par >= 30100.0) return INT16_MIN;
    const double ntc = 1.0 / (1.0 / par - 1.0 / 30100.0);
    const double t = 1.0 / (1.0 / 298.15 + log(ntc / 10000.0) / 3435.0) - 273.15;
    return (int)lround(t * 10);
}
