// The TF card log's rows and file names: plain C, host-tested.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t t_s;                // since this boot
    uint32_t unix_time;          // real time, 0 if the clock isn't set
    int sim;                     // 0 real; made up: 1 as a test, 2 used as if real (a build's setting)
    int pack_mv, pack_pct, pack_ma;
    const char *input;           // none, battery, usb
    int vbus_mv, ibus_ma, in_ma, ibat_ma, vsys_mv;
    int light_mv, light_ma, light_mw;
    int cell_c10, die_c10;       // INT16_MIN: no reading
    int chg_state, fault0, fault1, ts_flags;
    bool efuse_pg, lights_off, pole_off;
    int wled_preset, wled_bri, wled_fps;  // -1: WLED not heard
    int fin_follow;              // 1, 0, -1 not known
    uint32_t dropped;            // rows lost so far (the card fell behind or failed)
    int chg_limit_ma;            // the charger's input limit
} log_sample_t;

extern const char LOG_HEADER[];

// One CSV row with its newline. Returns its length, or 0 if it doesn't fit.
size_t log_row(char *out, size_t cap, const log_sample_t *s);

// A row's real time and the pack's charge. False for the header, a row with no
// real time or no charge reading, and test readings; rows used as if real only
// count while as_real is true (a build with the simulation on).
bool log_parse_charge(const char *row, uint32_t *unix_time, int *pct, bool as_real);

// A past reading into a history of n columns, newest at n - 1, slice_s seconds each.
void log_history_add(int8_t *cols, int n, float slice_s, uint32_t now, uint32_t t, int pct);

// The PCF85063's seven time registers (BCD; year from 2000). 0 from the
// registers if its oscillator stopped or they aren't a real date.
uint32_t rtc_regs_to_unix(const uint8_t r[7]);
void rtc_unix_to_regs(uint32_t t, uint8_t r[7]);

// The number in a log file's name (L00042.CSV: 42), or -1 if it isn't one.
int log_file_number(const char *name);
