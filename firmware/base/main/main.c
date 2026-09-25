// Nibbles base station (prototype): anchors the ESP-NOW channel, shows the
// eyes' status in the log, and sends preset commands from its buttons.
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nibbles_espnow.h"
#include "nvs_flash.h"

static const char *TAG = "base";

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t eyes_mac[6];
static bool have_eyes;
static nl_eye_telemetry_t eyes;
static int64_t eyes_us;

static void on_packet(const uint8_t mac[6], const nl_radio_hdr_t *h, const uint8_t *pl, size_t len)
{
    if (h->type == NL_MSG_EYE_TELEMETRY && len == sizeof(nl_eye_telemetry_t) && h->role == NL_ROLE_EYE_LEADER) {
        taskENTER_CRITICAL(&lock);
        memcpy(&eyes, pl, sizeof(eyes));
        memcpy(eyes_mac, mac, 6);
        have_eyes = true;
        eyes_us = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
    }
}

static void send_cmd(nl_op_t op, int arg)
{
    uint8_t mac[6];
    taskENTER_CRITICAL(&lock);
    const bool known = have_eyes;
    memcpy(mac, eyes_mac, 6);
    taskEXIT_CRITICAL(&lock);
    if (!known) {
        ESP_LOGW(TAG, "no eyes heard yet; command not sent");
        return;
    }
    nl_cmd_t cmd = { .target = NL_TARGET_EYES, .op = op, .arg = (int16_t)arg };
    uint8_t status = 0xFF;
    const int64_t t0 = esp_timer_get_time();
    const esp_err_t err = nl_espnow_command(mac, &cmd, &status, CMD_TRIES, CMD_TIMEOUT_MS);
    const float ms = (esp_timer_get_time() - t0) / 1000.0f;
    const char *what = op == NL_OP_PRESET_NEXT ? "next" : op == NL_OP_PRESET_PREV ? "previous" : "set";
    if (err == ESP_OK) ESP_LOGI(TAG, "eyes %s preset: acked in %.1f ms (status %d)", what, ms, status);
    else ESP_LOGW(TAG, "eyes %s preset: no ack after %d tries", what, CMD_TRIES);
}

static void send_preset(nl_op_t op) { send_cmd(op, 0); }
static void send_preset_index(int index) { send_cmd(NL_OP_PRESET_SET, index); }

static void buttons_task(void *arg)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_NEXT_GPIO) | (1ULL << BTN_PREV_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    int prev_next = 1, prev_prev = 1;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        const int n = gpio_get_level(BTN_NEXT_GPIO), p = gpio_get_level(BTN_PREV_GPIO);
        if (n == 0 && prev_next == 1) send_preset(NL_OP_PRESET_NEXT);
        if (p == 0 && prev_prev == 1) send_preset(NL_OP_PRESET_PREV);
        prev_next = n;
        prev_prev = p;
    }
}

// Serial commands over USB, for testing without pressing buttons:
// "next" / "prev" (same as the buttons), "set N".
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
        else if (line[0]) ESP_LOGW(TAG, "commands: next, prev, set N");
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
        .handler = on_packet,
        .core = 0,
    };
    ESP_ERROR_CHECK(nl_espnow_start(&cfg));
    xTaskCreate(buttons_task, "buttons", 4096, NULL, 4, NULL);
    xTaskCreate(console_task, "console", 4096, NULL, 4, NULL);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        nl_espnow_stats_t st;
        nl_espnow_get_stats(&st);
        nl_eye_telemetry_t e;
        taskENTER_CRITICAL(&lock);
        const bool fresh = have_eyes && esp_timer_get_time() - eyes_us < 2000000;
        e = eyes;
        taskEXIT_CRITICAL(&lock);
        if (fresh) {
            ESP_LOGI(TAG, "ch %d | eyes: preset %d/%d, %s, %s, fps %.1f / %.1f, late %d / %d, %.0f bpm, %.1f dB, hype %.2f | radio rx %lu tx %lu fail %lu",
                     st.channel, e.preset, e.preset_count, state_name(e.state), e.linked ? "linked" : "NOT linked",
                     e.fps_x10[0] / 10.0f, e.fps_x10[1] / 10.0f, e.late_frames[0], e.late_frames[1], e.tempo_bpm,
                     e.level_db, e.hype, (unsigned long)st.rx, (unsigned long)st.tx, (unsigned long)st.tx_fail);
        } else {
            ESP_LOGI(TAG, "ch %d | no eyes heard | radio rx %lu tx %lu fail %lu", st.channel, (unsigned long)st.rx,
                     (unsigned long)st.tx, (unsigned long)st.tx_fail);
        }
    }
}
