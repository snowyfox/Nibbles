// The TF card log's rows and file names: plain C, host-tested.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t t_s;                // since this boot (the base has no clock set)
    bool sim;                    // made-up readings ("sim on")
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
} log_sample_t;

extern const char LOG_HEADER[];

// One CSV row with its newline. Returns its length, or 0 if it doesn't fit.
size_t log_row(char *out, size_t cap, const log_sample_t *s);

// The number in a log file's name (L00042.CSV: 42), or -1 if it isn't one.
int log_file_number(const char *name);
