// The main board <-> mirror board cable. See mirror_link.h.
#include "mirror_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs.h"
#include "ota.h"

static const char *TAG = "mirror";

// ------------------------------------------------------------------ saved settings
static int get_u8(const char *key)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open("mirror", NVS_READONLY, &h) != ESP_OK) return 0;
    nvs_get_u8(h, key, &v);
    nvs_close(h);
    return v;
}

static bool set_u8(const char *key, uint8_t v)
{
    nvs_handle_t h;
    if (nvs_open("mirror", NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = nvs_set_u8(h, key, v) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool mirror_is_mirror(void) { return get_u8("role") == 1; }
bool mirror_set_role(bool mirror) { return set_u8("role", mirror ? 1 : 0); }
bool mirror_link_enabled(void) { return get_u8("link") == 1; }
bool mirror_link_enable(bool on) { return set_u8("link", on ? 1 : 0); }

// ------------------------------------------------------------------ the line
static bool uart_ready;

static void uart_up(int tx, int rx)
{
    const uart_config_t c = {
        .baud_rate = MIRROR_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (uart_driver_install(MIRROR_UART, 4096, 4096, 0, NULL, 0) != ESP_OK) return;
    uart_param_config(MIRROR_UART, &c);
    uart_set_pin(MIRROR_UART, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    gpio_pullup_en(rx);   // an unplugged cable reads idle, not noise
    uart_ready = true;
}

static void send_line(const char *line)
{
    uart_write_bytes(MIRROR_UART, line, strlen(line));
    uart_write_bytes(MIRROR_UART, "\n", 1);
}

// One line, or false after timeout_ms. Bytes that can't be text (a board's
// boot banner at another speed, a cable being plugged in) are dropped.
static bool read_line(char *out, size_t len, int timeout_ms)
{
    const int64_t until = esp_timer_get_time() + timeout_ms * 1000LL;
    size_t n = 0;
    for (;;) {
        const int64_t left = until - esp_timer_get_time();
        if (left <= 0) return false;
        uint8_t c;
        if (uart_read_bytes(MIRROR_UART, &c, 1, pdMS_TO_TICKS(left / 1000 + 1)) != 1) continue;
        if (c == '\n') {
            out[n] = 0;
            if (n) return true;
            continue;
        }
        if (c < 0x20 || c > 0x7e) { n = 0; continue; }   // not text: start the line again
        if (n < len - 1) out[n++] = (char)c;
    }
}

// ------------------------------------------------------------------ main board
static volatile bool seen;            // the mirror answered the last hello
static char mirror_build[17];
static volatile bool push_now;
static volatile int push_pct = -1;    // sending: 0..100
static char last_result[48];
static uint8_t line_buf[OTA_LINE_MAX + 16];

// Waits for a line starting with want (other lines are skipped). The answer is in line_buf.
static bool expect(const char *want, int timeout_ms)
{
    const int64_t until = esp_timer_get_time() + timeout_ms * 1000LL;
    for (;;) {
        const int64_t left = until - esp_timer_get_time();
        if (left <= 0) return false;
        if (!read_line((char *)line_buf, sizeof(line_buf), (int)(left / 1000) + 1)) return false;
        if (!strncmp((char *)line_buf, want, strlen(want))) return true;
        if (!strncmp((char *)line_buf, "OTA ERR", 7)) return false;
    }
}

static bool push(void)
{
    const void *p;
    size_t size;
    if (!ota_running_image(&p, &size)) { snprintf(last_result, sizeof(last_result), "no image to send"); return false; }
    const esp_partition_t *part = p;
    ESP_LOGI(TAG, "sending this image (%u bytes, build %s) to the mirror", (unsigned)size, ota_build_id());
    uart_flush_input(MIRROR_UART);
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "ota begin %u", (unsigned)size);
    send_line(cmd);
    push_pct = 0;
    if (!expect("OTA READY", 60000)) { snprintf(last_result, sizeof(last_result), "mirror not ready"); return false; }
    static uint8_t chunk[OTA_CHUNK];
    static char line[OTA_LINE_MAX];
    const int64_t t0 = esp_timer_get_time();
    for (size_t off = 0; off < size;) {
        const size_t n = size - off < OTA_CHUNK ? size - off : OTA_CHUNK;
        if (esp_partition_read(part, off, chunk, n) != ESP_OK) { snprintf(last_result, sizeof(last_result), "flash read failed"); return false; }
        int len = snprintf(line, sizeof(line), "ota data %u %08lx ", (unsigned)off, (unsigned long)ota_crc32(chunk, n));
        size_t b64 = 0;
        mbedtls_base64_encode((unsigned char *)line + len, sizeof(line) - len, &b64, chunk, n);
        bool ok = false;
        for (int tries = 0; tries < 5 && !ok; tries++) {
            send_line(line);
            if (expect("OTA OK", 3000)) ok = (size_t)strtoul((char *)line_buf + 7, NULL, 10) == off + n;
            else if (!strncmp((char *)line_buf, "OTA ERR", 7)) ESP_LOGW(TAG, "mirror: %s", (char *)line_buf);
        }
        if (!ok) { snprintf(last_result, sizeof(last_result), "failed at %u%%", (unsigned)(off * 100 / size)); send_line("ota abort"); return false; }
        off += n;
        push_pct = (int)(off * 100 / size);
    }
    send_line("ota end");
    if (!expect("OTA DONE", 15000)) { snprintf(last_result, sizeof(last_result), "mirror refused the image"); return false; }
    snprintf(last_result, sizeof(last_result), "sent in %d s", (int)((esp_timer_get_time() - t0) / 1000000));
    ESP_LOGI(TAG, "mirror took the image (%s); it restarts into it", last_result);
    return true;
}

static void main_task(void *arg)
{
    char tried[17] = "";   // the build we last tried to replace: once per boot, so a mirror that keeps failing isn't hammered
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MIRROR_HELLO_MS));
        if (ota_busy()) continue;   // this board is being updated: don't hand out the old image
        uart_flush_input(MIRROR_UART);
        send_line("hello");
        char build[17] = "", state[8] = "";
        const bool heard = expect("MIRROR ", 300) && sscanf((char *)line_buf, "MIRROR %16s %7s", build, state) == 2;
        if (heard && !seen) ESP_LOGI(TAG, "mirror found: build %s", build);
        if (!heard && seen) ESP_LOGI(TAG, "mirror gone");
        seen = heard;
        if (!heard) continue;
        memcpy(mirror_build, build, sizeof(mirror_build));
        const bool differs = strcmp(build, ota_build_id()) != 0;
        if ((push_now || (differs && strcmp(build, tried))) && !strcmp(state, "ready")) {
            push_now = false;
            memcpy(tried, build, sizeof(tried));
            if (!push()) ESP_LOGW(TAG, "mirror update: %s", last_result);
            push_pct = -1;
        }
    }
}

void mirror_link_main_start(void)
{
    if (!mirror_link_enabled()) return;
    uart_up(MIRROR_MAIN_TX_GPIO, MIRROR_MAIN_RX_GPIO);
    if (!uart_ready) { ESP_LOGE(TAG, "no UART for the mirror"); return; }
    ESP_LOGI(TAG, "mirror cable on GPIO %d (out) / %d (in) at %d baud", MIRROR_MAIN_TX_GPIO, MIRROR_MAIN_RX_GPIO, MIRROR_BAUD);
    xTaskCreate(main_task, "mirror", 4096, NULL, 3, NULL);
}

void mirror_link_push(void) { push_now = true; }

void mirror_link_text(char *out, size_t len)
{
    if (mirror_is_mirror()) snprintf(out, len, "this is the mirror; main %s", mirror_main_heard() ? "heard" : "not heard");
    else if (!uart_ready) snprintf(out, len, "mirror cable off (\"mirror link on\")");
    else if (push_pct >= 0) snprintf(out, len, "Mirror update %d%%", push_pct);
    else if (!seen) snprintf(out, len, "no mirror%s%s", last_result[0] ? "; last update: " : "", last_result);
    else snprintf(out, len, "mirror build %s%s%s%s", mirror_build, strcmp(mirror_build, ota_build_id()) ? " (differs)" : " (same)",
                  last_result[0] ? "; last update: " : "", last_result);
}

// ------------------------------------------------------------------ mirror board
static volatile int64_t main_us;

bool mirror_main_heard(void) { return main_us && esp_timer_get_time() - main_us < 3 * MIRROR_HELLO_MS * 1000LL; }

static void reply(void *ctx, const char *line) { send_line(line); }

static void side_task(void *arg)
{
    static char line[OTA_LINE_MAX + 16];
    for (;;) {
        if (!read_line(line, sizeof(line), 1000)) continue;
        main_us = esp_timer_get_time();
        if (!strcmp(line, "hello")) {
            char text[48];
            snprintf(text, sizeof(text), "MIRROR %s %s", ota_build_id(), ota_busy() ? "busy" : "ready");
            send_line(text);
        } else ota_command(line, reply, (void *)1);   // the cable (ctx non-NULL)
    }
}

void mirror_link_side_start(void)
{
    uart_up(MIRROR_SIDE_TX_GPIO, MIRROR_SIDE_RX_GPIO);
    if (!uart_ready) { ESP_LOGE(TAG, "no UART for the main board"); return; }
    ESP_LOGI(TAG, "mirror: listening for the main board on GPIO %d (in) / %d (out)", MIRROR_SIDE_RX_GPIO, MIRROR_SIDE_TX_GPIO);
    xTaskCreate(side_task, "mirror", 6144, NULL, 3, NULL);
}
