// Fin sensor node: reads a fin's hinge angle from an MT6701 magnetic angle
// sensor (analog output) and broadcasts it to WLED over ESP-NOW.
//
// Calibration: put the fin at its up limit and send "cal up" over USB serial,
// then at its down limit and send "cal down". Stored in NVS. "side port" /
// "side starboard" says which fin this sensor is; "cal clear" forgets.
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nibbles_espnow.h"
#include "nibbles_link.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "fins";

static adc_oneshot_unit_handle_t adc;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static float smooth = -1;            // smoothed reading, ADC counts
static int fin = NL_FIN_STARBOARD;   // which fin this sensor reads
static int cal_up = -1, cal_down = -1;

static void load_settings(void)
{
    nvs_handle_t h;
    if (nvs_open("fins", NVS_READONLY, &h) != ESP_OK) return;
    int32_t v;
    if (nvs_get_i32(h, "side", &v) == ESP_OK) fin = v;
    if (nvs_get_i32(h, "up", &v) == ESP_OK) cal_up = v;
    if (nvs_get_i32(h, "down", &v) == ESP_OK) cal_down = v;
    nvs_close(h);
}

static void save_settings(void)
{
    nvs_handle_t h;
    if (nvs_open("fins", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, "side", fin);
    nvs_set_i32(h, "up", cal_up);
    nvs_set_i32(h, "down", cal_down);
    nvs_commit(h);
    nvs_close(h);
}

static float reading(void)
{
    taskENTER_CRITICAL(&lock);
    const float r = smooth;
    taskEXIT_CRITICAL(&lock);
    return r;
}

// Calibrated angle in centidegrees: +9000 at the up limit, -9000 at the down
// limit (a little past either end is allowed).
static int16_t angle_cdeg(float raw)
{
    if (cal_up < 0 || cal_down < 0 || cal_up == cal_down || raw < 0) return NL_FIN_NONE;
    float a = -9000.0f + 18000.0f * (raw - cal_down) / (float)(cal_up - cal_down);
    if (a > 10000) a = 10000;
    if (a < -10000) a = -10000;
    return (int16_t)a;
}

static void sensor_task(void *arg)
{
    for (;;) {
        int sum = 0, n = 0;
        for (int i = 0; i < SAMPLES_PER_READ; i++) {
            int v;
            if (adc_oneshot_read(adc, SENSOR_ADC_CHANNEL, &v) == ESP_OK) { sum += v; n++; }
        }
        if (n) {
            const float r = (float)sum / n;
            taskENTER_CRITICAL(&lock);
            smooth = smooth < 0 ? r : smooth + SMOOTH_ALPHA * (r - smooth);
            taskEXIT_CRITICAL(&lock);
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}

static void send_task(void *arg)
{
    for (;;) {
        const float r = reading();
        nl_fin_pose_t p = {
            .angle_cdeg = { NL_FIN_NONE, NL_FIN_NONE },
            .raw = { NL_FIN_RAW_NONE, NL_FIN_RAW_NONE },
        };
        p.angle_cdeg[fin] = angle_cdeg(r);
        p.raw[fin] = r < 0 ? NL_FIN_RAW_NONE : (uint16_t)(r + 0.5f);
        nl_espnow_broadcast(NL_MSG_FIN_POSE, &p, sizeof(p));
        vTaskDelay(pdMS_TO_TICKS(SEND_MS));
    }
}

static void command(const char *line)
{
    const float r = reading();
    if (!strcmp(line, "cal up")) { cal_up = (int)(r + 0.5f); save_settings(); ESP_LOGI(TAG, "up limit = %d", cal_up); }
    else if (!strcmp(line, "cal down")) { cal_down = (int)(r + 0.5f); save_settings(); ESP_LOGI(TAG, "down limit = %d", cal_down); }
    else if (!strcmp(line, "cal clear")) { cal_up = cal_down = -1; save_settings(); ESP_LOGI(TAG, "calibration cleared"); }
    else if (!strcmp(line, "side port")) { fin = NL_FIN_PORT; save_settings(); ESP_LOGI(TAG, "this sensor reads the port fin"); }
    else if (!strcmp(line, "side starboard")) { fin = NL_FIN_STARBOARD; save_settings(); ESP_LOGI(TAG, "this sensor reads the starboard fin"); }
    else if (line[0]) ESP_LOGW(TAG, "commands: cal up, cal down, cal clear, side port, side starboard");
}

static void console_task(void *arg)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) vTaskDelete(NULL);
    usb_serial_jtag_vfs_use_driver();
    char line[48];
    int n = 0;
    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) continue;
        if (c == '\r' || c == '\n') { line[n] = 0; command(line); n = 0; }
        else if (n < (int)sizeof(line) - 1) line[n++] = (char)c;
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    load_settings();

    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &adc));
    adc_oneshot_chan_cfg_t ccfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, SENSOR_ADC_CHANNEL, &ccfg));

    const nl_espnow_config_t radio = {
        .role = NL_ROLE_FINS,
        .side = fin == NL_FIN_PORT ? NL_SIDE_PORT : NL_SIDE_STARBOARD,
        .fw = FINS_FW_BUILD,
        .anchor = false,
        .anchor_channel = START_CHANNEL,
        .anchor_fallback_ms = 0,   // never anchor: only follows WLED's channel
        .handler = NULL,
        .core = 0,
    };
    ESP_ERROR_CHECK(nl_espnow_start(&radio));
    xTaskCreate(sensor_task, "sensor", 3072, NULL, 5, NULL);
    xTaskCreate(send_task, "send", 3072, NULL, 4, NULL);
    xTaskCreate(console_task, "console", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "%s fin; calibration up %d, down %d", fin == NL_FIN_PORT ? "port" : "starboard", cal_up, cal_down);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        nl_espnow_stats_t st;
        nl_espnow_get_stats(&st);
        const float r = reading();
        const int16_t a = angle_cdeg(r);
        char ang[16];
        if (a == NL_FIN_NONE) snprintf(ang, sizeof(ang), "uncalibrated");
        else snprintf(ang, sizeof(ang), "%+.1f deg", a / 100.0f);
        ESP_LOGI(TAG, "%s fin: raw %.0f, %s | radio ch %d %s, tx %lu fail %lu", fin == NL_FIN_PORT ? "port" : "starboard",
                 r, ang, st.channel, st.locked ? "locked" : "scanning", (unsigned long)st.tx, (unsigned long)st.tx_fail);
    }
}
