// The power board, on its own I2C bus from the 3.49's header (SDA 1, SCL 2,
// INT 3): the BQ25798 charger (0x6B), the INA226 on the lights' output
// (0x40), the TCA9534 expander (0x20: P0 LIGHT_OFF, P1 PACK_OFF, P3
// EFUSE_PG), and the dock detect line (GPIO 5). The keypad boards share the
// bus (keypad.c). Until the board exists, "sim on" over serial fills in
// made-up readings for the screen; those are never broadcast.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    bool sim;                 // made-up readings ("sim on")
    bool have_charger, have_monitor, have_expander;
    bool docked;              // dock detect high, or the charger sees VBUS
    // charger (BQ25798)
    bool charger_ok;          // configured and read back as written
    uint8_t chg_state;        // chg_state_t
    int vbus_mv, ibus_ma;     // dock input
    int vbat_mv, ibat_ma;     // pack (ibat > 0 charging)
    int vsys_mv;
    int die_c10, cell_c10;    // charger die and cells (NTC), x10; INT16_MIN = no reading
    uint8_t fault0, fault1;   // REG20, REG21
    uint8_t ts_flags;         // REG1F: cold, cool, warm, hot
    // lights (INA226 after the eFuse)
    int light_mv, light_ma, light_mw;
    bool efuse_pg;            // eFuse power good
    bool lights_off, pack_off; // what the expander is driving
    // derived
    int pack_mv;              // best pack voltage reading (0 = none)
    int pack_pct;             // -1 = no reading
    int runtime_min;          // on the pack at the present load; -1 = not on the pack or no load
    uint8_t input;            // input_kind_t: what feeds the LEMO input
    int input_pct;            // an external battery's charge from its voltage; -1 = not a battery
    int in_ma;                // the LEMO's whole current (charger + light bus), worked out
    int in_split;             // per mille of it going to the charger; -1 = nothing comes in
    int pack_ma;              // the pack's whole current, + charging, - discharging, worked out
    bool starved;             // plugged in, not full, and the pack gets (almost) nothing
} power_status_t;

esp_err_t power_start(void);
void power_get(power_status_t *out);
i2c_master_bus_handle_t power_bus(void);   // shared with the keypads (NULL until started)
void power_sim(bool on);
// Lights' eFuse on/off (LIGHT_OFF). The pack's ideal diode (PACK_OFF) is
// only switched off while docked, and comes back on by itself when the dock goes.
void power_set_lights(bool on);
void power_set_pack(bool on);
