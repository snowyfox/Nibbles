// Power arithmetic with no hardware access (host-tested in test/host):
// state of charge from the pack voltage, runtime, and the power board's
// register conversions.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PACK_CELLS          4
#define PACK_WH             80      // usable energy of the 4S1P Vapcell F63 pack (docs/power.md)
#define PACK_MOHM           100     // pack, BMS, fuse and wiring to the board (estimate, docs/power.md)

// State of charge in percent (0-100) from the pack voltage, corrected for the
// sag at current_ma drawn from the pack (positive = discharging).
int pack_percent(int pack_mv, int current_ma);

// What feeds the LEMO input, from its voltage: an external battery (a 4S pack
// like ours, at most 16.8 V) or a supply (a USB-C PD charger through a 20 V
// trigger board), or nothing.
#define INPUT_PRESENT_MV    5000    // below this: nothing connected
#define INPUT_SUPPLY_MV     17500   // above a full 4S pack: a supply
typedef enum { INPUT_NONE = 0, INPUT_BATTERY, INPUT_SUPPLY } input_kind_t;
input_kind_t input_kind(int vbus_mv);

// Minutes left at load_mw from pct, or -1 when the load is too small to say.
int runtime_minutes(int pct, int load_mw);

// INA226 with a 2 mohm shunt and calibration for a 1 mA current LSB.
#define INA226_SHUNT_MOHM   2
#define INA226_CURRENT_LSB_UA 1000
uint16_t ina226_calibration(void);                  // value for register 5
int ina226_bus_mv(uint16_t reg);                    // register 2: 1.25 mV per bit
int ina226_current_ma(uint16_t reg);                // register 4: signed, current LSB
int ina226_power_mw(uint16_t reg);                  // register 3: 25 x current LSB

// BQ25798 status and ADC registers.
typedef enum {
    CHG_NOT_CHARGING = 0, CHG_TRICKLE, CHG_PRECHARGE, CHG_FAST, CHG_TAPER, CHG_RESERVED, CHG_TOPOFF, CHG_DONE,
} chg_state_t;
chg_state_t bq_charge_state(uint8_t reg1c);         // REG1C bits 7:5
const char *bq_charge_state_name(chg_state_t s);
int bq_ts_tenths_pct(uint16_t reg3f);               // TS as a percentage of REGN, x10
int bq_die_tenths_c(uint16_t reg41);                // die temperature x10 (0.5 C per bit, signed)
// The cells' temperature from the TS reading, for the power board's
// 5.23k / 30.1k divider from REGN and a 103AT (B 3435) NTC; x10, or INT16_MIN if open/shorted.
int ts_cell_tenths_c(int ts_tenths_pct);
