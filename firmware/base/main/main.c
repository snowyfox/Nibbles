// Nibbles base station: reads the power board (power.c) and the keypads
// (keypad.c), runs Live / Standby from the keys (controls.c), shows it all on
// its screen, sends preset commands and bumps to the eyes and WLED (and from
// USB serial), tells WLED the pack voltage, and anchors the ESP-NOW channel
// when there is no WLED usermod.
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "controls.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "keypad.h"
#include "nibbles_espnow.h"
#include "nvs_flash.h"
#include "power.h"
#include "power_math.h"

static const char *TAG = "base";

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t eyes_mac[6];
static bool have_eyes;
static nl_eye_telemetry_t eyes;
static int64_t eyes_us;
static uint8_t wled_mac[6];
static bool have_wled;
static nl_wled_telemetry_t wled;
static int64_t wled_us;
static nl_wled_stream_t wled_stream;
static int64_t wled_stream_us;
static nl_fin_state_t fin_state;
static nl_wled_layers_t wled_layers;   // WLED's layer slots (NL_MSG_WLED_LAYERS)
static int64_t wled_layers_us;
static bool have_fins;
static int64_t fins_us;

static void on_packet(const uint8_t mac[6], const nl_radio_hdr_t *h, const uint8_t *pl, size_t len)
{
    if (h->type == NL_MSG_EYE_TELEMETRY && len == sizeof(nl_eye_telemetry_t) && h->role == NL_ROLE_EYE_LEADER) {
        taskENTER_CRITICAL(&lock);
        memcpy(&eyes, pl, sizeof(eyes));
        memcpy(eyes_mac, mac, 6);
        have_eyes = true;
        eyes_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    } else if (h->type == NL_MSG_WLED_TELEMETRY && len == sizeof(nl_wled_telemetry_t) && h->role == NL_ROLE_WLED) {
        taskENTER_CRITICAL(&lock);
        memcpy(&wled, pl, sizeof(wled));
        memcpy(wled_mac, mac, 6);
        have_wled = true;
        wled_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    } else if (h->type == NL_MSG_WLED_STREAM && len == sizeof(nl_wled_stream_t) && h->role == NL_ROLE_WLED) {
        taskENTER_CRITICAL(&lock);
        memcpy(&wled_stream, pl, sizeof(wled_stream));
        wled_stream_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    } else if (h->type == NL_MSG_WLED_LAYERS && len == sizeof(nl_wled_layers_t) && h->role == NL_ROLE_WLED) {
        taskENTER_CRITICAL(&lock);
        memcpy(&wled_layers, pl, sizeof(wled_layers));
        wled_layers_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    } else if (h->type == NL_MSG_FIN_STATE && len == sizeof(nl_fin_state_t) && h->role == NL_ROLE_WLED) {
        taskENTER_CRITICAL(&lock);
        memcpy(&fin_state, pl, sizeof(fin_state));
        have_fins = true;
        fins_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    }
}

static void send_cmd_to(nl_target_t target, nl_op_t op, int arg)
{
    const char *who = target == NL_TARGET_WLED ? "wled" : "eyes";
    uint8_t mac[6];
    taskENTER_CRITICAL(&lock);
    const bool known = target == NL_TARGET_WLED ? have_wled : have_eyes;
    memcpy(mac, target == NL_TARGET_WLED ? wled_mac : eyes_mac, 6);
    taskEXIT_CRITICAL(&lock);
    if (!known) {
        ESP_LOGW(TAG, "%s not heard yet; command not sent", who);
        display_event("%s not heard yet", who);
        return;
    }
    nl_cmd_t cmd = { .target = target, .op = op, .arg = (int16_t)arg };
    uint8_t status = 0xFF;
    const int64_t t0 = esp_timer_get_time();
    const esp_err_t err = nl_espnow_command(mac, &cmd, &status, CMD_TRIES, CMD_TIMEOUT_MS);
    const float ms = (esp_timer_get_time() - t0) / 1000.0f;
    static const char *ops[] = { "?", "set preset", "next preset", "previous preset", "set brightness", "step brightness",
                                 "auto presets", "reactivity", "fin following", "set layer", "set palette" };
    const char *what = op <= NL_OP_SLOT_PALETTE ? ops[op] : "?";
    static const char *statuses[] = { "ok", "unsupported", "bad preset", "busy" };
    const char *st = status < 4 ? statuses[status] : "?";
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s %s %d: acked in %.1f ms (status %d)", who, what, arg, ms, status);
        display_event("%s %s: %s, %.1f ms", who, what, st, ms);
    } else {
        ESP_LOGW(TAG, "%s %s: no ack after %d tries", who, what, CMD_TRIES);
        display_event("%s %s: no answer", who, what);
    }
}

static void send_preset(nl_op_t op) { send_cmd_to(NL_TARGET_EYES, op, 0); }
static void send_preset_index(int index) { send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_SET, index); }

// ------------------------------------------------------------------ bumps
// Bumps are broadcast: START, HOLD every NL_BUMP_KEEPALIVE_MS while held,
// STOP. One slot per receiver (the eyes and WLED each filter on their own
// target bit), so the two can show different things at once, as TAKE's
// preview does. Keys, pads, the bump button and serial all use the slots.
enum { SLOT_EYES = 0, SLOT_WLED = 1, SLOT_FINS = 2, SLOTS };
typedef struct {
    nl_bump_t b;
    bool on;
    int64_t next_hold;
} bump_slot_t;
static bump_slot_t slots[SLOTS];
static const uint8_t slot_target[SLOTS] = { NL_TARGET_EYES, NL_TARGET_WLED, NL_TARGET_FINS };
static uint16_t bump_id;
static SemaphoreHandle_t bump_mutex;

// intensity: 255 (full) for flashes; a scene bump's palette + 1.
static void slot_start(int i, uint8_t action, int arg, uint8_t intensity)
{
    xSemaphoreTake(bump_mutex, portMAX_DELAY);
    slots[i].b = (nl_bump_t){ .id = ++bump_id, .target = slot_target[i],
                              .action = action, .phase = NL_BUMP_START, .intensity = intensity, .arg = (int16_t)arg };
    slots[i].on = true;
    slots[i].next_hold = esp_timer_get_time() + NL_BUMP_KEEPALIVE_MS * 1000LL;
    nl_espnow_broadcast(NL_MSG_BUMP, &slots[i].b, sizeof(nl_bump_t));
    xSemaphoreGive(bump_mutex);
}

static void slot_stop(int i)
{
    xSemaphoreTake(bump_mutex, portMAX_DELAY);
    if (slots[i].on) {
        slots[i].b.phase = NL_BUMP_STOP;
        slots[i].on = false;
        nl_espnow_broadcast(NL_MSG_BUMP, &slots[i].b, sizeof(nl_bump_t));
    }
    xSemaphoreGive(bump_mutex);
}

// target: NL_TARGET_* mask
static void bump_start(uint8_t target, uint8_t action, int arg)
{
    if (target & NL_TARGET_EYES) slot_start(SLOT_EYES, action, arg, 255);
    if (target & NL_TARGET_WLED) slot_start(SLOT_WLED, action, arg, 255);
}

static void bump_stop(uint8_t target)
{
    if (target & NL_TARGET_EYES) slot_stop(SLOT_EYES);
    if (target & NL_TARGET_WLED) slot_stop(SLOT_WLED);
}

static void bump_keepalive(void)
{
    const int64_t now = esp_timer_get_time();
    xSemaphoreTake(bump_mutex, portMAX_DELAY);
    for (int i = 0; i < SLOTS; i++) {
        if (!slots[i].on || now < slots[i].next_hold) continue;
        slots[i].b.phase = NL_BUMP_HOLD;
        slots[i].next_hold = now + NL_BUMP_KEEPALIVE_MS * 1000LL;
        nl_espnow_broadcast(NL_MSG_BUMP, &slots[i].b, sizeof(nl_bump_t));
    }
    xSemaphoreGive(bump_mutex);
}

static uint8_t bump_showing(void)  // for the screen
{
    return slots[SLOT_WLED].on ? slots[SLOT_WLED].b.action : slots[SLOT_EYES].on ? slots[SLOT_EYES].b.action : 0;
}

// ------------------------------------------------------------------ actions
// Touch taps, keypad actions and so on go to the actions task, in order:
// commands wait for their acks, which the display and keypad tasks mustn't.
static QueueHandle_t actions;
static volatile bool pad_flash, pad_black;
static volatile int pad_preset;  // preset being held from the grid (1-based), 0 = none

typedef struct {
    int kind;                     // display_action_t, or ACTION_CTL
    int value;
    ctl_action_t ctl;
} action_t;
#define ACTION_CTL 100

// The keypad's state; keys arrive from the keypad task, touch and serial.
static ctl_t ctl;
static SemaphoreHandle_t ctl_mutex;

static void run_ctl_actions(const ctl_action_t *a, int n)
{
    for (int i = 0; i < n; i++) {
        const action_t act = { ACTION_CTL, 0, a[i] };
        if (xQueueSend(actions, &act, pdMS_TO_TICKS(50)) != pdTRUE) ESP_LOGW(TAG, "actions queue full");
    }
}

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void on_key(ctl_key_t key, bool down)
{
    ctl_action_t a[CTL_MAX_ACTIONS];
    xSemaphoreTake(ctl_mutex, portMAX_DELAY);
    const int n = ctl_key(&ctl, key, down, now_ms(), a);
    xSemaphoreGive(ctl_mutex);
    ESP_LOGI(TAG, "key %s %s", ctl_key_name(key), down ? "down" : "up");
    run_ctl_actions(a, n);
}

static void on_touch(display_action_t a, bool pressed, int value)
{
    const action_t act = { a, value, { 0 } };
    if (a == DISPLAY_PAD_FLASH) pad_flash = pressed;
    else if (a == DISPLAY_PAD_BLACKOUT) pad_black = pressed;
    else if (a == DISPLAY_HOLD_PRESET) {
        if (pressed) pad_preset = value;
        else if (pad_preset == value) pad_preset = 0;
    } else if (a == DISPLAY_KEY) {
        if (value == KEY_NONE) {   // the target box: step to the next target on release
            if (!pressed) {
                static const ctl_key_t order[] = { KEY_ALL, KEY_BODY, KEY_EYES, KEY_FINS };
                const ctl_key_t k = order[(ctl.target + 1) % 4];
                on_key(k, true);
                on_key(k, false);
            }
        } else {
            on_key((ctl_key_t)value, pressed);
        }
    }
    else if (pressed || a == DISPLAY_SLIDE_EYES || a == DISPLAY_SLIDE_WLED) xQueueSend(actions, &act, 0);
}

// Preset N (1-based) on both: WLED preset id N, eye preset index N - 1.
static void send_preset_both(int n)
{
    send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_SET, n);
    send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_SET, n - 1);
}

static void do_ctl_action(const ctl_action_t *a)
{
    switch (a->type) {
        case ACT_PRESET:
            // BODY: the WLED preset without its eye settings; ALL lets them through
            if (a->target == TGT_BODY) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_SET, a->value | (a->bump ? 0 : NL_PRESET_NO_EYES));
            else if (a->target == TGT_FINS) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_SET, NL_FIN_PRESET_BASE + a->value);
            else send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_SET, a->value - 1);
            break;
        case ACT_FINS:       send_cmd_to(NL_TARGET_WLED, NL_OP_FIN_FOLLOW, a->value); break;
        // Fin presets are WLED presets NL_FIN_PRESET_BASE + n (the base itself: follow the body).
        case ACT_BRIGHTNESS: send_cmd_to(NL_TARGET_WLED, NL_OP_BRIGHTNESS_SET, a->value); break;
        case ACT_SLOT_LOOK:    send_cmd_to(NL_TARGET_WLED, NL_OP_SLOT_LOOK, nl_slot_arg(a->target, (uint8_t)a->value)); break;
        case ACT_SLOT_PALETTE: send_cmd_to(NL_TARGET_WLED, NL_OP_SLOT_PALETTE, nl_slot_arg(a->target, (uint8_t)a->value)); break;
        case ACT_BUMP:
            if (a->target == TGT_FINS && a->bump == NL_BUMP_PRESET) slot_start(SLOT_FINS, a->bump, NL_FIN_PRESET_BASE + a->value, 255);
            else if (a->bump == NL_BUMP_SCENE) slot_start(a->target == TGT_FINS ? SLOT_FINS : SLOT_WLED, a->bump, a->value, a->extra);
            else if (a->target == TGT_FINS) slot_start(SLOT_FINS, a->bump, a->value, 255);   // palette, layer: a slot arg
            else slot_start(a->target == TGT_EYES ? SLOT_EYES : SLOT_WLED, a->bump, a->value, 255);
            break;
        case ACT_BUMP_END:   slot_stop(a->target == TGT_EYES ? SLOT_EYES : a->target == TGT_FINS ? SLOT_FINS : SLOT_WLED); break;
    }
}

static void actions_task(void *arg)
{
    action_t a;
    for (;;) {
        if (xQueueReceive(actions, &a, portMAX_DELAY) != pdTRUE) continue;
        switch (a.kind) {
            case ACTION_CTL:         do_ctl_action(&a.ctl); break;
            case DISPLAY_TAP_EYES:   send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_NEXT, 0); break;
            case DISPLAY_TAP_WLED:   send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_NEXT, 0); break;
            // The eyes follow WLED's master brightness, so their slider sets that.
            case DISPLAY_SLIDE_EYES: send_cmd_to(NL_TARGET_WLED, NL_OP_BRIGHTNESS_SET, a.value); break;
            case DISPLAY_SLIDE_WLED: send_cmd_to(NL_TARGET_WLED, NL_OP_BRIGHTNESS_SET, a.value); break;
            case DISPLAY_TAP_PRESET: send_preset_both(a.value); break;
            case DISPLAY_SET_AUTO:   send_cmd_to(NL_TARGET_EYES, NL_OP_AUTO_CYCLE, a.value); break;
            case DISPLAY_SET_REACT:  send_cmd_to(NL_TARGET_EYES, NL_OP_REACTIVITY, a.value); break;
            case DISPLAY_SET_FINS:   send_cmd_to(NL_TARGET_WLED, NL_OP_FIN_FOLLOW, a.value); break;
            case DISPLAY_HOLD_EYES: {
                taskENTER_CRITICAL(&lock);
                const bool on = eyes.flags & NL_EYE_AUTO_CYCLE;
                taskEXIT_CRITICAL(&lock);
                send_cmd_to(NL_TARGET_EYES, NL_OP_AUTO_CYCLE, !on);
                break;
            }
            default: break;
        }
    }
}

// Hold a bump for ms (used by the serial "bump" command, on its own task so
// the console can take commands while a bump is held).
typedef struct {
    uint8_t target, action;
    int arg, ms;
} timed_bump_t;
static QueueHandle_t timed_bumps;

static void bump_for(uint8_t target, uint8_t action, int arg, int ms)
{
    const timed_bump_t b = { target, action, arg, ms };
    if (xQueueSend(timed_bumps, &b, 0) != pdTRUE) ESP_LOGW(TAG, "a bump is already being held");
}

static void timed_bump_task(void *arg)
{
    timed_bump_t b;
    for (;;) {
        if (xQueueReceive(timed_bumps, &b, portMAX_DELAY) != pdTRUE) continue;
        bump_start(b.target, b.action, b.arg);
        vTaskDelay(pdMS_TO_TICKS(b.ms));
        bump_stop(b.target);
        ESP_LOGI(TAG, "bump %d held %d ms", b.action, b.ms);
        display_event("bump %d held %d ms", b.action, b.ms);
    }
}

// The board's own keys and the touch pads; also sends every bump's keepalives.
static void buttons_task(void *arg)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_NEXT_GPIO) | (1ULL << BTN_BUMP_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    int prev_next = 1;
    uint8_t held = 0, held_target = 0;  // bump this task started
    int held_arg = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        const int n = gpio_get_level(BTN_NEXT_GPIO);
        if (n == 0 && prev_next == 1) {
            const action_t a = { DISPLAY_TAP_EYES, 0, { 0 } };
            xQueueSend(actions, &a, 0);
        }
        prev_next = n;
        // The bump key and the FLASH pad flash; the BLACKOUT pad wins over both;
        // a held preset-grid button bumps its preset on the eyes and WLED.
        const int preset = pad_preset;
        const uint8_t want = pad_black ? NL_BUMP_BLACKOUT
                           : (gpio_get_level(BTN_BUMP_GPIO) == 0 || pad_flash) ? NL_BUMP_FLASH
                           : preset ? NL_BUMP_PRESET : 0;
        const int want_arg = want == NL_BUMP_PRESET ? preset : 0;
        if (want != held || want_arg != held_arg) {
            if (held) bump_stop(held_target);
            held_target = want == NL_BUMP_PRESET ? (NL_TARGET_EYES | NL_TARGET_WLED) : BTN_BUMP_TARGET;
            if (want) bump_start(held_target, want, want_arg);
            held = want;
            held_arg = want_arg;
        }
        bump_keepalive();
    }
}

// ------------------------------------------------------------------ console
// Serial commands over USB, for testing without pressing buttons:
// "next" / "prev" / "set N" (eye presets), "wled next" / "wled set N",
// "bri N" / "wled bri N" (brightness 0..255; "+N" / "-N" steps),
// "[eyes|wled] bump flash|black|preset N ms" (hold a bump for ms; without a
// prefix it goes to both; preset numbers are 1-based on both),
// "wled slot look S N" / "wled slot pal S N" (WLED layer slot S: layer preset N, 0 off; palette N),
// "preset N" (preset N on both, as a grid tap), "page N" (0 main, 1 grid, 2 details),
// "key NAME [down|up]" (a keypad key; tapped without down/up), "keys" (list),
// "leds on|off" (print the key LED colours when they change),
// "sim on|off" (made-up power readings), "lights on|off", "power" (log the power board).
static void console_key(const char *args)
{
    char name[16] = "";
    char how[8] = "";
    sscanf(args, "%15s %7s", name, how);
    const ctl_key_t k = ctl_key_by_name(name);
    if (k == KEY_NONE) {
        ESP_LOGW(TAG, "no key '%s' (try: keys)", name);
        return;
    }
    if (!strcmp(how, "down")) on_key(k, true);
    else if (!strcmp(how, "up")) on_key(k, false);
    else {
        on_key(k, true);
        vTaskDelay(pdMS_TO_TICKS(60));
        on_key(k, false);
    }
}

static void log_power(void)
{
    power_status_t p;
    power_get(&p);
    ESP_LOGI(TAG, "power%s: charger %s%s, monitor %s, expander %s, %s; pack %d mV %d%% (%d mA), dock %d mV %d mA, "
             "%s; lights %d mV %d mA %d mW, eFuse %s%s; cells %d.%d C, die %d.%d C; faults %02x %02x, TS %x",
             p.sim ? " (SIMULATED)" : "", p.have_charger ? "yes" : "no", p.have_charger && !p.charger_ok ? " (NOT SET UP)" : "",
             p.have_monitor ? "yes" : "no", p.have_expander ? "yes" : "no", p.docked ? "docked" : "not docked",
             p.pack_mv, p.pack_pct, p.ibat_ma, p.vbus_mv, p.ibus_ma, bq_charge_state_name((chg_state_t)p.chg_state),
             p.light_mv, p.light_ma, p.light_mw, p.efuse_pg ? "ok" : "off", p.lights_off ? ", LIGHTS OFF" : "",
             p.cell_c10 / 10, abs(p.cell_c10 % 10), p.die_c10 / 10, abs(p.die_c10 % 10), p.fault0, p.fault1, p.ts_flags);
}

// "leds on": print the key LED colours whenever they change, as one line,
// "LEDS rrggbb,rrggbb,..." in ctl_key_t order from KEY_NUM1, so the simulator
// can light its keys like the real ones while it drives this base.
static volatile bool leds_echo;

// Screenshots print from their own task (~20 KB to 300 KB of text), so the
// console keeps reading keys meanwhile. Notified with 1 (raw) or 2 (rle).
static TaskHandle_t shot_task_handle;

static void shot_task(void *arg)
{
    for (;;) display_screenshot(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == 2);
}

static void console_task(void *arg)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) vTaskDelete(NULL);
    usb_serial_jtag_vfs_use_driver();
    char line[48];
    size_t n = 0;
    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) continue;
        if (c != '\n' && c != '\r') {
            if (n < sizeof(line) - 1) line[n++] = (char)c;
            continue;
        }
        line[n] = 0;
        n = 0;
        if (!strcmp(line, "next")) send_preset(NL_OP_PRESET_NEXT);
        else if (!strcmp(line, "prev")) send_preset(NL_OP_PRESET_PREV);
        else if (!strncmp(line, "set ", 4)) send_preset_index(atoi(line + 4));
        else if (!strcmp(line, "shot") || !strcmp(line, "shot rle")) {
            if (!shot_task_handle) xTaskCreate(shot_task, "shot", 4096, NULL, 2, &shot_task_handle);
            if (shot_task_handle) xTaskNotify(shot_task_handle, line[4] ? 2 : 1, eSetValueWithOverwrite);
        }
        else if (!strncmp(line, "page ", 5)) display_show_page(atoi(line + 5));
        else if (!strncmp(line, "key ", 4)) console_key(line + 4);
        else if (!strcmp(line, "keys")) {
            for (int k = 1; k < KEY_COUNT; k++) printf("%s ", ctl_key_name((ctl_key_t)k));
            printf("\n");
        }
        else if (!strcmp(line, "sim on") || !strcmp(line, "sim off")) power_sim(line[5] == 'n');
        else if (!strcmp(line, "lights on") || !strcmp(line, "lights off")) power_set_lights(line[8] == 'n');
        else if (!strcmp(line, "power")) log_power();
        else if (!strcmp(line, "leds on") || !strcmp(line, "leds off")) leds_echo = line[6] == 'n';
        else if (!strncmp(line, "react ", 6))
            send_cmd_to(NL_TARGET_EYES, NL_OP_REACTIVITY,
                        !strcmp(line + 6, "beats") ? NL_REACT_BEATS : !strcmp(line + 6, "peaks") ? NL_REACT_PEAKS : NL_REACT_PRESET);
        else if (!strncmp(line, "preset ", 7)) send_preset_both(atoi(line + 7));
        else if (!strcmp(line, "fins on") || !strcmp(line, "fins off") || !strcmp(line, "fins toggle"))
            send_cmd_to(NL_TARGET_WLED, NL_OP_FIN_FOLLOW, line[6] == 'n' ? 1 : line[6] == 'f' ? 0 : 2);
        else if (!strcmp(line, "auto on") || !strcmp(line, "auto off")) send_cmd_to(NL_TARGET_EYES, NL_OP_AUTO_CYCLE, line[6] == 'n');
        else if (!strncmp(line, "bri ", 4))  // the eyes follow WLED's brightness: same as "wled bri"
            send_cmd_to(NL_TARGET_WLED, line[4] == '+' || line[4] == '-' ? NL_OP_BRIGHTNESS_STEP : NL_OP_BRIGHTNESS_SET, atoi(line + 4));
        else if (!strncmp(line, "wled bri ", 9)) send_cmd_to(NL_TARGET_WLED, line[9] == '+' || line[9] == '-' ? NL_OP_BRIGHTNESS_STEP : NL_OP_BRIGHTNESS_SET, atoi(line + 9));
        else if (!strcmp(line, "wled next")) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_NEXT, 0);
        else if (!strncmp(line, "wled set ", 9)) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_SET, atoi(line + 9));
        else if (!strncmp(line, "wled slot look ", 15) || !strncmp(line, "wled slot pal ", 14)) {   // "wled slot look S N", "wled slot pal S N"
            const bool look = line[10] == 'l';
            int sl = 0, v = 0;
            if (sscanf(line + (look ? 15 : 14), "%d %d", &sl, &v) == 2)
                send_cmd_to(NL_TARGET_WLED, look ? NL_OP_SLOT_LOOK : NL_OP_SLOT_PALETTE, nl_slot_arg((uint8_t)sl, (uint8_t)v));
        }
        else if (strstr(line, "bump ")) {
            const char *b = strstr(line, "bump ") + 5;
            uint8_t target = !strncmp(line, "eyes ", 5) ? NL_TARGET_EYES
                           : !strncmp(line, "wled ", 5) ? NL_TARGET_WLED : NL_TARGET_EYES | NL_TARGET_WLED;
            int preset = 0, ms = 0;
            if (!strncmp(b, "flash ", 6)) bump_for(target, NL_BUMP_FLASH, 0, atoi(b + 6));
            else if (!strncmp(b, "black ", 6)) bump_for(target, NL_BUMP_BLACKOUT, 0, atoi(b + 6));
            else if (!strncmp(b, "preset ", 7) && sscanf(b + 7, "%d %d", &preset, &ms) == 2) {
                bump_for(target, NL_BUMP_PRESET, preset, ms);
            }
        } else if (line[0]) ESP_LOGW(TAG, "commands: next, prev, set N, wled next, wled set N, [wled] bri N|+N|-N, auto on|off, react preset|beats|peaks, preset N, fins on|off|toggle, page N, shot, [eyes|wled] bump flash|black MS, [eyes|wled] bump preset N MS, wled slot look|pal S N, key NAME [down|up], keys, leds on|off, sim on|off, lights on|off, power");
    }
}

static const char *state_name(uint8_t s)
{
    static const char *names[] = { "awake", "drowsy", "asleep", "waking" };
    return s < 4 ? names[s] : "?";
}

// Preset names as they turn up in telemetry, for showing what Standby holds.
static char wled_names[256][NL_NAME_LEN];
static char eye_names[64][NL_NAME_LEN];

static void name_of(char out[NL_NAME_LEN], char cache[][NL_NAME_LEN], int count, int preset)
{
    out[0] = 0;
    if (preset > 0 && preset < count) memcpy(out, cache[preset], NL_NAME_LEN);
    out[NL_NAME_LEN - 1] = 0;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    const nl_espnow_config_t cfg = {
        .role = NL_ROLE_BASE,
        .side = NL_SIDE_NONE,
        .fw = BASE_FW_BUILD,
        .anchor = BASE_IS_ANCHOR,
        .anchor_channel = BASE_ANCHOR_CHANNEL,
        .anchor_fallback_ms = BASE_ANCHOR_FALLBACK_MS,
        .handler = on_packet,
        .core = 0,
    };
    bump_mutex = xSemaphoreCreateMutex();
    ctl_mutex = xSemaphoreCreateMutex();
    ctl_init(&ctl, KEYPAD_AUTOLOCK_MS);
    actions = xQueueCreate(16, sizeof(action_t));
    timed_bumps = xQueueCreate(1, sizeof(timed_bump_t));
    if (display_start(on_touch) != ESP_OK) ESP_LOGE(TAG, "no display; carrying on without it");
    ESP_ERROR_CHECK(nl_espnow_start(&cfg));
    if (power_start() != ESP_OK) ESP_LOGE(TAG, "no power board bus");
    if (keypad_start(on_key) != ESP_OK) ESP_LOGE(TAG, "no keypad task");
    xTaskCreate(buttons_task, "buttons", 4096, NULL, 4, NULL);
    xTaskCreate(actions_task, "actions", 4096, NULL, 4, NULL);
    xTaskCreate(timed_bump_task, "timed_bump", 3072, NULL, 4, NULL);
    xTaskCreate(console_task, "console", 4096, NULL, 4, NULL);

    int64_t next_battery = 0;
    for (int n = 0;; n++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        const int64_t now = esp_timer_get_time();
        taskENTER_CRITICAL(&lock);
        const bool fresh = have_eyes && now - eyes_us < 2000000;
        const nl_eye_telemetry_t e = eyes;
        const nl_wled_telemetry_t w = wled;
        const bool wfresh = have_wled && now - wled_us < 2000000;
        const nl_wled_stream_t ws = wled_stream;
        const bool sfresh = wled_stream_us && now - wled_stream_us < 3000000 && ws.active;
        const nl_fin_state_t fs = fin_state;
        const bool ffresh = have_fins && now - fins_us < 2000000;
        const nl_wled_layers_t wl = wled_layers;
        const bool lfresh = wled_layers_us && now - wled_layers_us < 2000000 && wl.count >= NL_SLOT_COUNT;
        taskEXIT_CRITICAL(&lock);
        // The layers' palettes and looks, for the keys' LEDs and the screen (-1: not known)
        ctl_layers_t layers = { -1, -1, -1, -1 };
        if (lfresh) {
            layers.body_pal = wl.slot[NL_SLOT_BODY].on ? wl.slot[NL_SLOT_BODY].palette : -1;
            layers.fins_pal = wl.slot[NL_SLOT_FINS].on ? wl.slot[NL_SLOT_FINS].palette : -1;
            layers.body_layer = wl.slot[NL_SLOT_BODY_OVER].on ? wl.slot[NL_SLOT_BODY_OVER].look : 0;
            layers.fins_layer = wl.slot[NL_SLOT_FINS_OVER].on ? wl.slot[NL_SLOT_FINS_OVER].look : 0;
        }

        // What the shark shows, for Live; remember the names that come with it.
        const ctl_scene_t live = { .body = wfresh ? w.preset : 0, .eyes = fresh ? e.preset + 1 : 0,
                                   .fins = ffresh ? fs.fin_preset : -1 };
        if (wfresh && w.preset && w.name[0]) memcpy(wled_names[w.preset], w.name, NL_NAME_LEN);
        if (fresh && e.preset + 1 < 64 && e.name[0]) memcpy(eye_names[e.preset + 1], e.name, NL_NAME_LEN);
        ctl_action_t acts[CTL_MAX_ACTIONS];
        uint32_t rgb[KEY_COUNT];
        xSemaphoreTake(ctl_mutex, portMAX_DELAY);
        ctl_set_live(&ctl, &live, wfresh ? w.bri : 0, ffresh ? fs.follow : -1);
        ctl_set_layers(&ctl, &layers);
        const int nacts = ctl_tick(&ctl, now_ms(), acts);
        ctl_led_colors(&ctl, now_ms(), rgb);
        const ctl_t c = ctl;
        xSemaphoreGive(ctl_mutex);
        run_ctl_actions(acts, nacts);
        keypad_set_leds(rgb);
        static uint32_t echoed[KEY_COUNT];
        static bool echo_was;
        if (leds_echo && (!echo_was || memcmp(echoed, rgb, sizeof(echoed)))) {
            char text[8 * KEY_COUNT + 8];
            int len = snprintf(text, sizeof(text), "LEDS ");
            for (int k = 1; k < KEY_COUNT; k++) len += snprintf(text + len, sizeof(text) - len, "%s%06lx", k > 1 ? "," : "", (unsigned long)rgb[k]);
            printf("%s\n", text);
            memcpy(echoed, rgb, sizeof(echoed));
        }
        echo_was = leds_echo;

        power_status_t pw;
        power_get(&pw);
        // WLED's battery-aware current limit wants the voltage feeding the
        // pole and the lights' current; real readings only, never simulated.
        if (!pw.sim && pw.have_monitor && now >= next_battery) {
            next_battery = now + BATTERY_BROADCAST_MS * 1000LL;
            const nl_battery_t b = { .pack_mv = (uint16_t)pw.light_mv, .current_ma = pw.light_ma, .flags = NL_BATT_CURRENT };
            nl_espnow_broadcast(NL_MSG_BATTERY, &b, sizeof(b));
        }

        if (n % 5) continue;  // the screen every 250 ms
        nl_espnow_stats_t st;
        nl_espnow_get_stats(&st);
        display_status_t ds = {
            .channel = st.channel,
            .locked = st.locked,
            .anchoring = st.anchoring,
            .rx = st.rx,
            .tx = st.tx,
            .tx_fail = st.tx_fail,
            .eyes_fresh = fresh,
            .eyes = e,
            .wled_fresh = wfresh,
            .wled = w,
            .stream_fresh = sfresh,
            .stream = ws,
            .bump_action = bump_showing(),
            .fins_fresh = ffresh,
            .fins = fs,
            .power = pw,
            .live = c.live,
            .standby = c.standby,
            .brightness = c.brightness,
            .target = c.target,
            .page = c.page,
            .layers = c.layers,
            .diff = ctl_standby_diff(&c),
            .keys_locked = c.locked,
            .take_preview = c.take_preview,
            .latch = c.down[KEY_LATCH],
        };
        name_of(ds.live_body, wled_names, 256, c.live.body);
        name_of(ds.live_eyes, eye_names, 64, c.live.eyes);
        name_of(ds.stby_body, wled_names, 256, c.standby.body);
        name_of(ds.stby_eyes, eye_names, 64, c.standby.eyes);
        name_of(ds.live_fins, wled_names, 256, c.live.fins > 0 ? NL_FIN_PRESET_BASE + c.live.fins : 0);
        name_of(ds.stby_fins, wled_names, 256, c.standby.fins > 0 ? NL_FIN_PRESET_BASE + c.standby.fins : 0);
        ds.fin_sensor = c.fin_sensor;
        {
            const char *pn = ctl_palette_name(&c, c.layers.body_pal);
            snprintf(ds.live_pal_name, sizeof(ds.live_pal_name), "%s", pn ? pn : "");
            ctl_standby_layers(&c, &ds.stby_layers);
            pn = ctl_palette_name(&c, ds.stby_layers.body_pal);
            snprintf(ds.stby_pal_name, sizeof(ds.stby_pal_name), "%s", pn ? pn : "");
        }
        const uint8_t found = keypad_boards_found();
        ds.keypads_found = __builtin_popcount(found);
        ds.keypads_total = keypad_board_count();
        display_update(&ds);
        if (n % 40) continue;  // log every 2 s
        char eyes_s[160] = "no eyes heard", wled_s[200] = "no WLED heard";
        if (fresh)
            snprintf(eyes_s, sizeof(eyes_s), "eyes: preset %d/%d, %s, %s, fps %.1f / %.1f, late %d / %d, %.0f bpm, hype %.2f",
                     e.preset, e.preset_count, state_name(e.state), e.linked ? "linked" : "NOT linked",
                     e.fps_x10[0] / 10.0f, e.fps_x10[1] / 10.0f, e.late_frames[0], e.late_frames[1], e.tempo_bpm, e.hype);
        if (wfresh)
            snprintf(wled_s, sizeof(wled_s), "wled: %s bri %d, preset %d, fx %d, %u fps, %u LEDs", w.on ? "on" : "off",
                     w.bri, w.preset, w.fx, w.fps, w.leds);
        if (wfresh && sfresh) {
            const size_t len = strlen(wled_s);
            snprintf(wled_s + len, sizeof(wled_s) - len, ", stream from %u.%u.%u.%u: %u stalls/min (%u slow), longest %u ms, %lu stalls total",
                     ws.source_ip[0], ws.source_ip[1], ws.source_ip[2], ws.source_ip[3], ws.stalls, ws.slow, ws.max_gap_ms,
                     (unsigned long)ws.stalls_total);
        }
        ESP_LOGI(TAG, "ch %d%s | %s | %s | radio rx %lu tx %lu fail %lu | live %d/%d standby %d/%d %s%s | keypads %d",
                 st.channel, st.anchoring ? " (anchor)" : st.locked ? "" : " (scanning)", eyes_s, wled_s,
                 (unsigned long)st.rx, (unsigned long)st.tx, (unsigned long)st.tx_fail, c.live.body, c.live.eyes,
                 c.standby.body, c.standby.eyes, ctl_target_name(c.target), c.locked ? " LOCKED" : "", ds.keypads_found);
        if (pw.have_charger || pw.have_monitor || pw.sim) log_power();
    }
}
