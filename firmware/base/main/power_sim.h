// Made-up power board readings: plain C, host-tested.
//
// SIM_FAST goes through every value the screen can show and every problem it can
// report in SIM_FAST_CYCLE_S. SIM_REALISTIC is festival nights in real time: the
// pack runs the show, external batteries get swapped in, then a USB-C charge at
// the hotel, with the charge, voltage and temperatures following from the load.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { SIM_OFF = 0, SIM_REALISTIC, SIM_FAST } sim_mode_t;

typedef enum { SIM_ON_PACK = 0, SIM_ON_EXTERNAL, SIM_AT_HOTEL } sim_phase_t;

#define SIM_FAST_CYCLE_S 120

typedef struct {
    bool docked, charger_ok, efuse_pg;
    uint8_t chg_state, fault0, fault1, ts_flags;
    int vbus_mv, ibus_ma, vbat_mv, ibat_ma, vsys_mv;
    int die_c10, cell_c10;
    int light_mv, light_ma, light_mw;
} sim_reading_t;

typedef struct {        // the realistic simulation's state
    double t_s, pack_pct, ext_pct, cell_c, die_c;
    sim_phase_t phase;
    int shows;          // times the pack has run the show since the hotel
    uint32_t noise;
} sim_state_t;

void sim_reset(sim_state_t *s);
// Moves it on dt_s seconds.
void sim_realistic(sim_state_t *s, double dt_s, sim_reading_t *out);
void sim_fast(double t_s, sim_reading_t *out);
// The resting voltage of a 4S pack at a charge (the inverse of pack_percent).
int sim_rest_mv(double pct);
