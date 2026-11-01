// Data logger on the 3.49's TF card: one CSV row a second, a new file each boot
// (/sd/nibbles/L00001.CSV and up). Everything happens on its own low-priority
// task, so a slow or failed card can't hold up the keys, radio or screen; with
// no card it retries every 30 s. Oldest files go when the card is nearly full.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t logger_start(void);

// What the shark is doing, for the log (-1 where not known).
void logger_set_show(int wled_preset, int wled_bri, int wled_fps, int fin_follow);

typedef struct {
    bool enabled, mounted, open;
    int file_number;
    uint32_t rows, dropped;
    uint32_t max_write_ms;      // slowest flush to the card
    uint32_t card_mb, free_mb;
} logger_status_t;
void logger_get(logger_status_t *out);
void logger_enable(bool on);    // "log on|off": closes the file and unmounts when off
void logger_print_tail(int lines);   // the current file's last lines, to the console
