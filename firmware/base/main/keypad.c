#include "keypad.h"
#include "keypad_panel.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "keymap_text.h"
#include "led_strip.h"
#include "nvs.h"
#include "power.h"

static const char *TAG = "keypad";

typedef struct {
    uint8_t addr;
    uint8_t chip;                 // KEYPAD_TCA9534 or KEYPAD_PCF8574
    uint8_t keys;                 // switches fitted (8 on a column, 3 on the upper keypad)
    ctl_key_t map[8];             // logical key per switch (P0 = switch 1, the first LED)
} board_t;

static const board_t boards[] = KEYPAD_BOARDS;
#define NBOARDS ((int)(sizeof(boards) / sizeof(boards[0])))

// The key map in use: KEYPAD_BOARDS's, replaced per board by one saved in NVS
// (namespace "keymap", key "k" + the board's address in hex, e.g. "k38": the
// names, comma-separated). `keymap` on the console changes it; the change goes
// to `wanted` and the keypad task takes it, so a held key is let go cleanly.
#define KEYMAP_NVS "keymap"
static ctl_key_t map[NBOARDS][8];
static ctl_key_t wanted[NBOARDS][8];
static volatile bool map_changed;

static keypad_cb_t cb;
static led_strip_handle_t strip;
static int led_count;
static i2c_master_dev_handle_t devs[NBOARDS];
static volatile uint8_t found;
static uint32_t leds[KEY_COUNT];
static volatile bool leds_dirty = true;
static volatile int last_board = -1, last_switch;   // the switch pressed last (keymap last)
static volatile int64_t last_us;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

uint8_t keypad_boards_found(void) { return found; }
int keypad_board_count(void) { return NBOARDS; }

void keypad_set_leds(const uint32_t rgb[KEY_COUNT])
{
    taskENTER_CRITICAL(&lock);
    if (memcmp(leds, rgb, sizeof(leds))) {
        memcpy(leds, rgb, sizeof(leds));
        leds_dirty = true;
    }
    taskEXIT_CRITICAL(&lock);
}

static void set_pixel(int i, uint32_t c, int level)
{
    led_strip_set_pixel(strip, i, (c >> 16 & 255) * level / 255, (c >> 8 & 255) * level / 255, (c & 255) * level / 255);
}

static void show_leds(void)
{
    if (!strip) return;
    uint32_t rgb[KEY_COUNT];
    taskENTER_CRITICAL(&lock);
    memcpy(rgb, leds, sizeof(rgb));
    leds_dirty = false;
    taskEXIT_CRITICAL(&lock);
    // The chain runs through the boards in KEYPAD_BOARDS order, switch 1 first on each,
    // with the mirror panel after them, or before them with KEYPAD_PANEL_FIRST.
    // led[n]: the nth board with LEDs. A board the base doesn't find leaves its
    // panel row dark, so an unplugged board shows as a gap.
    int i = KEYPAD_PANEL_FIRST ? KEYPAD_PANEL_PIXELS : 0, nled = 0;
    int led[NBOARDS];
    for (int b = 0; b < NBOARDS; b++) {
        if (boards[b].chip != KEYPAD_TCA9534) continue;   // prototypes have no LEDs
        led[nled++] = b;
        for (int k = 0; k < boards[b].keys; k++, i++)
            set_pixel(i, map[b][k] ? rgb[map[b][k]] : 0, KEYPAD_LED_LEVEL);
    }
    if (KEYPAD_PANEL_FIRST) i = 0;
    for (int p = 0; p < KEYPAD_PANEL_PIXELS; p++, i++) {
        int b, k;
        uint32_t c = 0;
        if (keypad_panel_source(p, KEYPAD_PANEL_PIXELS, KEYPAD_PANEL_ROW, KEYPAD_PANEL_FLIP_ROWS,
                                KEYPAD_PANEL_FLIP_KEYS, &b, &k) &&
            b < nled && (found & 1 << led[b]) && k < boards[led[b]].keys && map[led[b]][k])
            c = rgb[map[led[b]][k]];
        set_pixel(i, c, KEYPAD_PANEL_LEVEL);
    }
    led_strip_refresh(strip);
}

static void keypad_task(void *arg)
{
    i2c_master_bus_handle_t bus;
    while (!(bus = power_bus())) vTaskDelay(pdMS_TO_TICKS(100));
    for (int b = 0; b < NBOARDS; b++) {
        const i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = boards[b].addr,
                                          .scl_speed_hz = POWER_I2C_HZ };
        i2c_master_bus_add_device(bus, &cfg, &devs[b]);
    }
    uint8_t raw[NBOARDS], stable[NBOARDS];  // input register, bits low = pressed
    uint8_t misses[NBOARDS] = { 0 };         // failed reads in a row
    memset(raw, 0xff, sizeof(raw));
    memset(stable, 0xff, sizeof(stable));
    int64_t next_probe = 0, next_leds = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(KEYPAD_POLL_MS));
        const int64_t now = esp_timer_get_time();
        if (map_changed) {   // a new key map: a key held on a switch that changed is let go first
            ctl_key_t next[NBOARDS][8];
            taskENTER_CRITICAL(&lock);
            memcpy(next, wanted, sizeof(next));
            map_changed = false;
            taskEXIT_CRITICAL(&lock);
            for (int b = 0; b < NBOARDS; b++)
                for (int k = 0; k < boards[b].keys; k++)
                    if (map[b][k] != next[b][k] && !(stable[b] & 1 << k) && map[b][k] && cb) cb(map[b][k], false);
            memcpy(map, next, sizeof(map));
            leds_dirty = true;
        }
        if (now >= next_probe) {   // boards plugged in later are picked up within 2 s
            next_probe = now + 2000000;
            for (int b = 0; b < NBOARDS; b++) {
                if (found & 1 << b) continue;
                // A probe alone isn't enough: a glitch on the bus can make an absent
                // address seem to answer. The board must also give a real read.
                if (i2c_master_probe(bus, boards[b].addr, 10) == ESP_OK) {
                    if (boards[b].chip == KEYPAD_PCF8574) {   // all pins high: weak pull-ups, read as inputs
                        const uint8_t all = 0xff;
                        i2c_master_transmit(devs[b], &all, 1, 10);
                    }
                    uint8_t reg = 0, in;
                    const esp_err_t err = boards[b].chip == KEYPAD_PCF8574
                                              ? i2c_master_receive(devs[b], &in, 1, 10)
                                              : i2c_master_transmit_receive(devs[b], &reg, 1, &in, 1, 10);
                    power_bus_after_read();
                    if (err != ESP_OK) continue;
                    found |= 1 << b;
                    misses[b] = 0;
                    ESP_LOGI(TAG, "keypad board 0x%02x found (%d keys)", boards[b].addr, boards[b].keys);
                    leds_dirty = true;
                }
            }
        }
        for (int b = 0; b < NBOARDS; b++) {
            if (!(found & 1 << b)) continue;
            // TCA9534: register 0 is the inputs. PCF8574: no registers, a plain read is
            // the pins (writing the 0 would drive every pin low).
            uint8_t reg = 0, in;
            const esp_err_t err = boards[b].chip == KEYPAD_PCF8574 ? i2c_master_receive(devs[b], &in, 1, 10)
                                                                   : i2c_master_transmit_receive(devs[b], &reg, 1, &in, 1, 10);
            power_bus_after_read();
            if (err != ESP_OK) {
                // one failed read keeps the keys as they were; three in a row is a lost board
                if (++misses[b] < 3) {
                    ESP_LOGW(TAG, "keypad board 0x%02x read failed (%s), %d of 3", boards[b].addr, esp_err_to_name(err), misses[b]);
                    continue;
                }
                ESP_LOGW(TAG, "keypad board 0x%02x lost (%s)", boards[b].addr, esp_err_to_name(err));
                found &= ~(1 << b);
                leds_dirty = true;   // its panel row goes dark
                // whatever it had down comes up
                for (int k = 0; k < boards[b].keys; k++)
                    if (!(stable[b] & 1 << k) && map[b][k] && cb) cb(map[b][k], false);
                raw[b] = stable[b] = 0xff;
                continue;
            }
            misses[b] = 0;
            // Debounce: a change counts once two reads in a row agree.
            const uint8_t settled = ~(in ^ raw[b]);           // bits that read the same twice
            const uint8_t changed = (stable[b] ^ in) & settled;
            raw[b] = in;
            if (!changed) continue;
            for (int k = 0; k < boards[b].keys; k++) {
                if (!(changed & 1 << k)) continue;
                const bool down = !(in & 1 << k);
                if (down) {
                    last_us = now;
                    last_switch = k;
                    last_board = b;
                }
                if (map[b][k] && cb) cb(map[b][k], down);
            }
            stable[b] ^= changed;
        }
        if (leds_dirty || now >= next_leds) {   // also refreshed now and then, in case a board was replugged
            next_leds = now + 1000000;
            show_leds();
        }
    }
}

static void map_load(void)
{
    for (int b = 0; b < NBOARDS; b++) memcpy(map[b], boards[b].map, sizeof(map[b]));
    nvs_handle_t h;
    if (nvs_open(KEYMAP_NVS, NVS_READONLY, &h) == ESP_OK) {
        for (int b = 0; b < NBOARDS; b++) {
            char key[8], text[200];
            size_t len = sizeof(text);
            snprintf(key, sizeof(key), "k%02x", boards[b].addr);
            if (nvs_get_str(h, key, text, &len) != ESP_OK) continue;
            if (keymap_parse(text, map[b], boards[b].keys))
                ESP_LOGI(TAG, "board 0x%02x: saved key map %s", boards[b].addr, text);
            else
                ESP_LOGW(TAG, "board 0x%02x: saved key map not understood, using the built-in one (%s)", boards[b].addr, text);
        }
        nvs_close(h);
    }
    memcpy(wanted, map, sizeof(wanted));
}

// Where the console's answer goes: the USB serial, or a radio answer (keypad_console).
static keypad_reply_t reply_fn;
static void *reply_ctx;
static SemaphoreHandle_t console_mutex;

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...)
{
    char line[240];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (reply_fn) reply_fn(reply_ctx, line);
    else printf("%s\n", line);
}

static void map_print(int b)
{
    char text[200];
    keymap_format(wanted[b], boards[b].keys, text, sizeof(text));
    say("KEYMAP %d 0x%02x%s %s%s", b + 1, boards[b].addr, boards[b].chip == KEYPAD_PCF8574 ? " (prototype)" : "", text,
           memcmp(wanted[b], boards[b].map, sizeof(ctl_key_t) * boards[b].keys) ? " (changed)" : "");
}

// Saves board b's map (or forgets it when it's the built-in one again). Console task only.
static bool map_save(int b)
{
    nvs_handle_t h;
    if (nvs_open(KEYMAP_NVS, NVS_READWRITE, &h) != ESP_OK) return false;
    char key[8], text[200];
    snprintf(key, sizeof(key), "k%02x", boards[b].addr);
    esp_err_t err;
    if (memcmp(wanted[b], boards[b].map, sizeof(ctl_key_t) * boards[b].keys)) {
        keymap_format(wanted[b], boards[b].keys, text, sizeof(text));
        err = nvs_set_str(h, key, text);
    } else {
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// "1"-"N" (the board's place in KEYPAD_BOARDS) or its address ("0x38").
static int board_arg(const char *s)
{
    char *end;
    const long v = strtol(s, &end, 0);
    if (*end) return -1;
    if (!strncmp(s, "0x", 2) || !strncmp(s, "0X", 2)) {
        for (int b = 0; b < NBOARDS; b++)
            if (boards[b].addr == v) return b;
        return -1;
    }
    return v >= 1 && v <= NBOARDS ? (int)v - 1 : -1;
}

// keymap colors: each switch's LED colour now (its key's), per board.
static void colors_print(void)
{
    uint32_t now[KEY_COUNT];
    taskENTER_CRITICAL(&lock);
    memcpy(now, leds, sizeof(now));
    taskEXIT_CRITICAL(&lock);
    for (int b = 0; b < NBOARDS; b++) {
        char text[8 * 8 + 1];
        int len = 0;
        text[0] = 0;
        for (int k = 0; k < boards[b].keys; k++) {
            const ctl_key_t key = wanted[b][k];
            len += snprintf(text + len, sizeof(text) - len, "%s%06lx", k ? "," : "",
                            (unsigned long)(key > KEY_NONE && key < KEY_COUNT ? now[key] & 0xffffff : 0));
        }
        say("KEYCOLORS %d %s", b + 1, text);
    }
}

// keymap last: the switch pressed last (to find which switch is which).
static void last_print(void)
{
    const int b = last_board, k = last_switch;
    if (b < 0) {
        say("KEYLAST none");
        return;
    }
    say("KEYLAST %d %d %s %lld", b + 1, k + 1, ctl_key_name(map[b][k]), (long long)((esp_timer_get_time() - last_us) / 1000));
}

static void found_print(void)
{
    char text[3 * 8 + 1] = "";
    int len = 0;
    for (int b = 0; b < NBOARDS && len < (int)sizeof(text) - 3; b++)
        len += snprintf(text + len, sizeof(text) - len, "%s%d", b ? "," : "", (found >> b) & 1);
    say("KEYFOUND %s", text);
}

static void console_line(const char *line)
{
    if (!strcmp(line, "keys")) {   // every key name, for keymap
        static char text[600];   // not on the caller's stack (the base's main loop answers the radio)
        int len = 0;
        text[0] = 0;
        for (int k = 1; k < KEY_COUNT && len < (int)sizeof(text) - 1; k++)
            len += snprintf(text + len, sizeof(text) - len, "%s%s", k > 1 ? " " : "", ctl_key_name((ctl_key_t)k));
        say("%s", text);
        return;
    }
    const char *args = line[6] ? line + 7 : "";
    char a[12] = "", s[12] = "", name[24] = "";
    const int n = sscanf(args, "%11s %11s %23s", a, s, name);
    if (n <= 0) {   // keymap: the whole map
        for (int b = 0; b < NBOARDS; b++) map_print(b);
        return;
    }
    if (n == 1 && !strcmp(a, "colors")) { colors_print(); return; }
    if (n == 1 && !strcmp(a, "last")) { last_print(); return; }
    if (n == 1 && !strcmp(a, "all")) {   // what the brains' /keys page shows
        for (int b = 0; b < NBOARDS; b++) map_print(b);
        colors_print();
        found_print();
        last_print();
        return;
    }
    if (n == 1 && !strcmp(a, "reset")) {   // keymap reset: back to KEYPAD_BOARDS for every board
        taskENTER_CRITICAL(&lock);
        for (int b = 0; b < NBOARDS; b++) memcpy(wanted[b], boards[b].map, sizeof(wanted[b]));
        map_changed = true;
        taskEXIT_CRITICAL(&lock);
        nvs_handle_t h;
        bool ok = nvs_open(KEYMAP_NVS, NVS_READWRITE, &h) == ESP_OK;
        if (ok) {
            ok = nvs_erase_all(h) == ESP_OK && nvs_commit(h) == ESP_OK;
            nvs_close(h);
        }
        say("KEYMAP reset to the built-in map%s", ok ? "" : " (could not clear the saved one)");
        for (int b = 0; b < NBOARDS; b++) map_print(b);
        return;
    }
    const int b = board_arg(a);
    if (b < 0) {
        say("KEYMAP no board '%s': 1-%d or its address (keymap lists them)", a, NBOARDS);
        return;
    }
    if (n == 1) {   // keymap B: one board
        map_print(b);
        return;
    }
    ctl_key_t keys[8];
    memcpy(keys, wanted[b], sizeof(keys));
    if (n == 2 && !strcmp(s, "reset")) {   // keymap B reset
        memcpy(keys, boards[b].map, sizeof(keys));
    } else if (n == 2 && strchr(s, ',')) {   // keymap B name,name,...: the whole board at once
        if (!keymap_parse(s, keys, boards[b].keys)) {
            say("KEYMAP board %d needs %d names, comma-separated, from: keys (or none)", b + 1, boards[b].keys);
            return;
        }
    } else if (n == 3) {   // keymap B S NAME: one switch
        const int sw = atoi(s);
        ctl_key_t k;
        if (sw < 1 || sw > boards[b].keys) {
            say("KEYMAP board %d has switches 1-%d", b + 1, boards[b].keys);
            return;
        }
        if (!keymap_key(name, &k)) {
            say("KEYMAP no key '%s' (keys lists them; none clears the switch)", name);
            return;
        }
        keys[sw - 1] = k;
    } else {
        say("KEYMAP use: keymap | keymap B | keymap B S NAME | keymap B name,name,... | keymap B reset | keymap reset");
        return;
    }
    taskENTER_CRITICAL(&lock);
    memcpy(wanted[b], keys, sizeof(keys));
    map_changed = true;
    taskEXIT_CRITICAL(&lock);
    if (!map_save(b)) say("KEYMAP could not save: the change lasts until a restart");
    map_print(b);
}

bool keypad_console(const char *line, keypad_reply_t reply, void *ctx)
{
    if (strcmp(line, "keys") && (strncmp(line, "keymap", 6) || (line[6] && line[6] != ' '))) return false;
    if (console_mutex) xSemaphoreTake(console_mutex, portMAX_DELAY);   // the USB serial and the radio
    reply_fn = reply;
    reply_ctx = ctx;
    console_line(line);
    reply_fn = NULL;
    if (console_mutex) xSemaphoreGive(console_mutex);
    return true;
}

esp_err_t keypad_start(keypad_cb_t on_key)
{
    cb = on_key;
    console_mutex = xSemaphoreCreateMutex();
    map_load();
    for (int b = 0; b < NBOARDS; b++)
        if (boards[b].chip == KEYPAD_TCA9534) led_count += boards[b].keys;
    led_count += KEYPAD_PANEL_PIXELS;   // the mirror panel, after or before the keys
    if (!led_count) led_count = 1;   // the driver wants at least one
    const led_strip_config_t sc = {
        .strip_gpio_num = PIN_KEY_LEDS,
        .max_leds = led_count,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rc = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10 * 1000 * 1000 };
    if (led_strip_new_rmt_device(&sc, &rc, &strip) != ESP_OK) {
        ESP_LOGE(TAG, "no LED driver; keys work without their lights");
        strip = NULL;
    }
    return xTaskCreate(keypad_task, "keypad", 4096, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
