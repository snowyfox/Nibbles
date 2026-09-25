// Nibbles base station (prototype): shows the eyes' and WLED's status on its
// screen and in the log, sends preset commands and bumps from its buttons (and
// USB serial), and anchors the ESP-NOW channel when there is no WLED usermod.
#include <stdlib.h>
#include <string.h>
#include "config.h"
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
#include "nibbles_espnow.h"
#include "nvs_flash.h"

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
    const char *what = op == NL_OP_PRESET_NEXT ? "next" : op == NL_OP_PRESET_PREV ? "previous" : "set";
    static const char *statuses[] = { "ok", "unsupported", "bad preset", "busy" };
    const char *st = status < 4 ? statuses[status] : "?";
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%s %s preset: acked in %.1f ms (status %d)", who, what, ms, status);
        display_event("%s %s preset: %s, %.1f ms", who, what, st, ms);
    } else {
        ESP_LOGW(TAG, "%s %s preset: no ack after %d tries", who, what, CMD_TRIES);
        display_event("%s %s preset: no answer", who, what);
    }
}

static void send_preset(nl_op_t op) { send_cmd_to(NL_TARGET_EYES, op, 0); }
static void send_preset_index(int index) { send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_SET, index); }

// Bumps are broadcast: START, HOLD every NL_BUMP_KEEPALIVE_MS while held, STOP.
static uint16_t bump_id;
static nl_bump_t bump;

static volatile uint8_t bump_held;  // action being held, for the screen
static SemaphoreHandle_t bump_mutex;  // the buttons and console tasks both send bumps

static void bump_send(uint8_t phase)
{
    xSemaphoreTake(bump_mutex, portMAX_DELAY);
    bump.phase = phase;
    bump_held = phase == NL_BUMP_STOP ? 0 : bump.action;
    nl_espnow_broadcast(NL_MSG_BUMP, &bump, sizeof(bump));
    xSemaphoreGive(bump_mutex);
}

static void bump_start(uint8_t target, uint8_t action, int arg)
{
    xSemaphoreTake(bump_mutex, portMAX_DELAY);
    bump = (nl_bump_t){ .id = ++bump_id, .target = target, .action = action, .intensity = 255, .arg = (int16_t)arg };
    xSemaphoreGive(bump_mutex);
    bump_send(NL_BUMP_START);
}

// Touch screen: taps go to the actions task (commands wait for acks, which
// the display task mustn't); pads are read by the buttons task.
static QueueHandle_t actions;
static volatile bool pad_flash, pad_black;

static void on_touch(display_action_t a, bool pressed)
{
    if (a == DISPLAY_PAD_FLASH) pad_flash = pressed;
    else if (a == DISPLAY_PAD_BLACKOUT) pad_black = pressed;
    else if (pressed) xQueueSend(actions, &a, 0);
}

static void actions_task(void *arg)
{
    display_action_t a;
    for (;;) {
        if (xQueueReceive(actions, &a, portMAX_DELAY) != pdTRUE) continue;
        if (a == DISPLAY_TAP_EYES) send_cmd_to(NL_TARGET_EYES, NL_OP_PRESET_NEXT, 0);
        else if (a == DISPLAY_TAP_WLED) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_NEXT, 0);
    }
}

// Hold a bump for ms (used by the serial "bump" command).
static void bump_for(uint8_t target, uint8_t action, int arg, int ms)
{
    bump_start(target, action, arg);
    for (int t = 0; t < ms; t += NL_BUMP_KEEPALIVE_MS) {
        vTaskDelay(pdMS_TO_TICKS(NL_BUMP_KEEPALIVE_MS));
        bump_send(NL_BUMP_HOLD);
    }
    bump_send(NL_BUMP_STOP);
    ESP_LOGI(TAG, "bump %d held %d ms", action, ms);
    display_event("bump %d held %d ms", action, ms);
}

static void buttons_task(void *arg)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_NEXT_GPIO) | (1ULL << BTN_BUMP_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    int prev_next = 1;
    uint8_t held = 0;  // bump this task is sending
    int64_t next_hold = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        const int n = gpio_get_level(BTN_NEXT_GPIO);
        if (n == 0 && prev_next == 1) {
            const display_action_t a = DISPLAY_TAP_EYES;
            xQueueSend(actions, &a, 0);
        }
        prev_next = n;
        // The bump key and the FLASH pad flash; the BLACKOUT pad wins over both.
        const uint8_t want = pad_black ? NL_BUMP_BLACKOUT
                           : (gpio_get_level(BTN_BUMP_GPIO) == 0 || pad_flash) ? NL_BUMP_FLASH : 0;
        const int64_t now = esp_timer_get_time();
        if (want != held) {
            if (held) bump_send(NL_BUMP_STOP);
            if (want) bump_start(BTN_BUMP_TARGET, want, 0);
            held = want;
            next_hold = now + NL_BUMP_KEEPALIVE_MS * 1000LL;
        } else if (held && now >= next_hold) {
            bump_send(NL_BUMP_HOLD);
            next_hold = now + NL_BUMP_KEEPALIVE_MS * 1000LL;
        }
    }
}

// Serial commands over USB, for testing without pressing buttons:
// "next" / "prev" / "set N" (eye presets), "wled next" / "wled set N",
// "[eyes|wled] bump flash|black|preset N ms" (hold a bump for ms; without a
// prefix, flash and blackout go to both and preset bumps to WLED).
static void console_task(void *arg)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) vTaskDelete(NULL);
    usb_serial_jtag_vfs_use_driver();
    char line[32];
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
        else if (!strcmp(line, "shot")) display_screenshot();
        else if (!strcmp(line, "wled next")) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_NEXT, 0);
        else if (!strncmp(line, "wled set ", 9)) send_cmd_to(NL_TARGET_WLED, NL_OP_PRESET_SET, atoi(line + 9));
        else if (strstr(line, "bump ")) {
            const char *b = strstr(line, "bump ") + 5;
            uint8_t target = !strncmp(line, "eyes ", 5) ? NL_TARGET_EYES
                           : !strncmp(line, "wled ", 5) ? NL_TARGET_WLED : NL_TARGET_EYES | NL_TARGET_WLED;
            int preset = 0, ms = 0;
            if (!strncmp(b, "flash ", 6)) bump_for(target, NL_BUMP_FLASH, 0, atoi(b + 6));
            else if (!strncmp(b, "black ", 6)) bump_for(target, NL_BUMP_BLACKOUT, 0, atoi(b + 6));
            else if (!strncmp(b, "preset ", 7) && sscanf(b + 7, "%d %d", &preset, &ms) == 2) {
                if (target == (NL_TARGET_EYES | NL_TARGET_WLED)) target = NL_TARGET_WLED;
                bump_for(target, NL_BUMP_PRESET, preset, ms);
            }
        } else if (line[0]) ESP_LOGW(TAG, "commands: next, prev, set N, wled next, wled set N, [eyes|wled] bump flash|black MS, [eyes|wled] bump preset N MS");
    }
}

static const char *state_name(uint8_t s)
{
    static const char *names[] = { "awake", "drowsy", "asleep", "waking" };
    return s < 4 ? names[s] : "?";
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
    actions = xQueueCreate(4, sizeof(display_action_t));
    if (display_start(on_touch) != ESP_OK) ESP_LOGE(TAG, "no display; carrying on without it");
    ESP_ERROR_CHECK(nl_espnow_start(&cfg));
    xTaskCreate(buttons_task, "buttons", 4096, NULL, 4, NULL);
    xTaskCreate(actions_task, "actions", 4096, NULL, 4, NULL);
    xTaskCreate(console_task, "console", 4096, NULL, 4, NULL);

    for (int n = 0;; n++) {
        vTaskDelay(pdMS_TO_TICKS(250));
        nl_espnow_stats_t st;
        nl_espnow_get_stats(&st);
        nl_eye_telemetry_t e;
        taskENTER_CRITICAL(&lock);
        const bool fresh = have_eyes && esp_timer_get_time() - eyes_us < 2000000;
        e = eyes;
        taskEXIT_CRITICAL(&lock);
        char eyes_s[160] = "no eyes heard", wled_s[96] = "no WLED heard";
        taskENTER_CRITICAL(&lock);
        const nl_wled_telemetry_t w = wled;
        const bool wfresh = have_wled && esp_timer_get_time() - wled_us < 2000000;
        taskEXIT_CRITICAL(&lock);
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
            .bump_action = bump_held,
        };
        display_update(&ds);
        if (n % 8) continue;  // log every 2 s
        if (fresh)
            snprintf(eyes_s, sizeof(eyes_s), "eyes: preset %d/%d, %s, %s, fps %.1f / %.1f, late %d / %d, %.0f bpm, hype %.2f",
                     e.preset, e.preset_count, state_name(e.state), e.linked ? "linked" : "NOT linked",
                     e.fps_x10[0] / 10.0f, e.fps_x10[1] / 10.0f, e.late_frames[0], e.late_frames[1], e.tempo_bpm, e.hype);
        if (wfresh)
            snprintf(wled_s, sizeof(wled_s), "wled: %s bri %d, preset %d, fx %d, %u fps, %u LEDs", w.on ? "on" : "off",
                     w.bri, w.preset, w.fx, w.fps, w.leds);
        ESP_LOGI(TAG, "ch %d%s | %s | %s | radio rx %lu tx %lu fail %lu", st.channel, st.anchoring ? " (anchor)" : st.locked ? "" : " (scanning)",
                 eyes_s, wled_s, (unsigned long)st.rx, (unsigned long)st.tx, (unsigned long)st.tx_fail);
    }
}
