// Firmware updates without USB: the text protocol (USB console, mirror cable)
// and the Wi-Fi upload. See ota.h.
#include "ota.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/base64.h"
#include "nvs.h"

static const char *TAG = "ota";

// ------------------------------------------------------------------ shared
static SemaphoreHandle_t lock;            // one writer at a time (serial or HTTP)
static char build_id[17];

// The update in progress: from the protocol (serial / mirror cable) or HTTP.
static volatile bool active;
static const esp_partition_t *target;
static esp_ota_handle_t handle;
static size_t total, written;
static int64_t last_us;
static const char *source = "";

uint32_t ota_crc32(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t crc = 0xffffffff;
    while (len--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) crc = crc >> 1 ^ (0xedb88320 & -(crc & 1));
    }
    return ~crc;
}

static void restart_cb(void *arg) { esp_restart(); }

static void restart_soon(void)
{
    static esp_timer_handle_t t;
    const esp_timer_create_args_t a = { .callback = restart_cb, .name = "ota_restart" };
    if (!t && esp_timer_create(&a, &t) != ESP_OK) esp_restart();
    esp_timer_start_once(t, 500 * 1000);
}

static void mark_good(void *arg)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "this image ran %d s: kept (%s)", OTA_GOOD_AFTER_S, run->label);
    }
}

void ota_start(void)
{
    if (!lock) lock = xSemaphoreCreateMutex();
    // esp_app_get_elf_sha256() stops at CONFIG_APP_RETRIEVE_LEN_ELF_SHA (9) digits
    const uint8_t *sha = esp_app_get_description()->app_elf_sha256;
    for (int i = 0; i < 8; i++) snprintf(build_id + 2 * i, 3, "%02x", sha[i]);
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(run, &st);
    ESP_LOGI(TAG, "running %s, build %s%s", run->label, build_id,
             st == ESP_OTA_IMG_PENDING_VERIFY ? ", new: kept if it runs " : "");
    static esp_timer_handle_t t;
    const esp_timer_create_args_t a = { .callback = mark_good, .name = "ota_good" };
    if (esp_timer_create(&a, &t) == ESP_OK) esp_timer_start_once(t, OTA_GOOD_AFTER_S * 1000000LL);
}

const char *ota_build_id(void) { return build_id; }

bool ota_running_image(const void **partition, size_t *size)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_pos_t pos = { .offset = run->address, .size = run->size };
    esp_image_metadata_t md;
    if (esp_image_get_metadata(&pos, &md) != ESP_OK) return false;
    *partition = run;
    *size = md.image_len;
    return true;
}

bool ota_busy(void)
{
    // a session nobody has fed for a minute is over
    if (active && esp_timer_get_time() - last_us > 60000000LL) {
        xSemaphoreTake(lock, portMAX_DELAY);
        if (active && esp_timer_get_time() - last_us > 60000000LL) {
            ESP_LOGW(TAG, "update from %s stalled at %u / %u: dropped", source, (unsigned)written, (unsigned)total);
            esp_ota_abort(handle);
            active = false;
        }
        xSemaphoreGive(lock);
    }
    return active;
}

void ota_progress_text(char *out, size_t len)
{
    if (!ota_busy()) { out[0] = 0; return; }
    snprintf(out, len, "Updating %s %d%%", source, total ? (int)(written * 100 / total) : 0);
}

// Starts a session (lock held by the caller). size 0: not known yet (HTTP chunked).
static esp_err_t begin(size_t size, const char *from)
{
    if (active) { esp_ota_abort(handle); active = false; }
    target = esp_ota_get_next_update_partition(NULL);
    if (!target) return ESP_ERR_NOT_FOUND;
    if (size > target->size) return ESP_ERR_INVALID_SIZE;
    // Erasing the whole size up front (a few seconds) keeps every write after it quick.
    const esp_err_t err = esp_ota_begin(target, size ? size : OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) return err;
    total = size;
    written = 0;
    source = from;
    last_us = esp_timer_get_time();
    active = true;
    ESP_LOGI(TAG, "update from %s: %u bytes into %s", from, (unsigned)size, target->label);
    return ESP_OK;
}

static esp_err_t finish(void)
{
    active = false;
    esp_err_t err = esp_ota_end(handle);   // checks the image (its SHA-256 and checksum)
    if (err == ESP_OK) err = esp_ota_set_boot_partition(target);
    if (err == ESP_OK) ESP_LOGI(TAG, "update from %s done (%u bytes): restarting into %s", source, (unsigned)written, target->label);
    else ESP_LOGE(TAG, "update from %s failed at the end: %s", source, esp_err_to_name(err));
    return err;
}

// ------------------------------------------------------------------ the text protocol
static void replyf(ota_reply_t reply, void *ctx, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void replyf(ota_reply_t reply, void *ctx, const char *fmt, ...)
{
    char text[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    reply(ctx, text);
}

bool ota_command(const char *line, ota_reply_t reply, void *ctx)
{
    if (strncmp(line, "ota ", 4)) return false;
    const char *cmd = line + 4;
    if (!lock) lock = xSemaphoreCreateMutex();

    if (!strncmp(cmd, "data ", 5)) {
        static uint8_t buf[OTA_CHUNK + 4];
        char *end;
        const unsigned long off = strtoul(cmd + 5, &end, 10);
        const unsigned long crc = strtoul(end, &end, 16);
        while (*end == ' ') end++;
        size_t n = 0;
        xSemaphoreTake(lock, portMAX_DELAY);
        if (!active) { xSemaphoreGive(lock); replyf(reply, ctx, "OTA ERR not started"); return true; }
        if (mbedtls_base64_decode(buf, sizeof(buf), &n, (const unsigned char *)end, strlen(end)) || n == 0 || n > OTA_CHUNK) {
            xSemaphoreGive(lock);
            replyf(reply, ctx, "OTA ERR %lu bad base64", off);
            return true;
        }
        if (ota_crc32(buf, n) != crc) {
            xSemaphoreGive(lock);
            replyf(reply, ctx, "OTA ERR %lu bad crc", off);
            return true;
        }
        if (off + n == written) {   // the sender missed our answer and sent it again
            xSemaphoreGive(lock);
            replyf(reply, ctx, "OTA OK %u", (unsigned)written);
            return true;
        }
        if (off != written) {
            const unsigned w = written;
            xSemaphoreGive(lock);
            replyf(reply, ctx, "OTA ERR %lu expected %u", off, w);
            return true;
        }
        const esp_err_t err = esp_ota_write(handle, buf, n);
        if (err == ESP_OK) { written += n; last_us = esp_timer_get_time(); }
        const unsigned w = written;
        xSemaphoreGive(lock);
        if (err == ESP_OK) replyf(reply, ctx, "OTA OK %u", w);
        else replyf(reply, ctx, "OTA ERR %lu %s", off, esp_err_to_name(err));
        return true;
    }
    if (!strncmp(cmd, "begin ", 6)) {
        const unsigned long size = strtoul(cmd + 6, NULL, 10);
        if (!size) { replyf(reply, ctx, "OTA ERR no size"); return true; }
        xSemaphoreTake(lock, portMAX_DELAY);
        const esp_err_t err = begin(size, ctx ? "cable" : "USB")   /* the console passes no ctx */;
        const char *label = target ? target->label : "?";
        xSemaphoreGive(lock);
        if (err == ESP_OK) replyf(reply, ctx, "OTA READY %s", label);
        else replyf(reply, ctx, "OTA ERR %s", esp_err_to_name(err));
        return true;
    }
    if (!strcmp(cmd, "end")) {
        xSemaphoreTake(lock, portMAX_DELAY);
        esp_err_t err = ESP_ERR_INVALID_STATE;
        if (active && written == total) err = finish();
        else if (active) { esp_ota_abort(handle); active = false; err = ESP_ERR_INVALID_SIZE; }
        xSemaphoreGive(lock);
        if (err == ESP_OK) {
            replyf(reply, ctx, "OTA DONE");
            restart_soon();
        } else replyf(reply, ctx, "OTA ERR %s", esp_err_to_name(err));
        return true;
    }
    if (!strcmp(cmd, "abort")) {
        xSemaphoreTake(lock, portMAX_DELAY);
        if (active) esp_ota_abort(handle);
        active = false;
        xSemaphoreGive(lock);
        replyf(reply, ctx, "OTA ABORTED");
        return true;
    }
    if (!strcmp(cmd, "status")) {
        const esp_partition_t *run = esp_ota_get_running_partition();
        esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
        esp_ota_get_state_partition(run, &st);
        const void *p;
        size_t size = 0;
        ota_running_image(&p, &size);
        replyf(reply, ctx, "OTA %s %u/%u, running %s build %s, %u bytes%s", ota_busy() ? "RECEIVING" : "IDLE", (unsigned)written,
               (unsigned)total, run->label, build_id, (unsigned)size, st == ESP_OTA_IMG_PENDING_VERIFY ? " (on trial)" : "");
        return true;
    }
    replyf(reply, ctx, "OTA ERR commands: begin SIZE, data OFF CRC B64, end, abort, status");
    return true;
}

// ------------------------------------------------------------------ Wi-Fi
// Off unless asked for: the base never joins a network by itself.
static volatile bool wifi_wanted;
static esp_netif_t *sta;
static httpd_handle_t httpd;
static char ip_text[16];

static esp_err_t http_info(httpd_req_t *req)
{
    const esp_app_desc_t *d = esp_app_get_description();
    char text[200];
    snprintf(text, sizeof(text), "Nibbles base %s, build %s, running %s\nUpdate: curl --data-binary @nibbles_base.bin http://%s/ota\n",
             d->version, build_id, esp_ota_get_running_partition()->label, ip_text);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, text);
}

static esp_err_t http_ota(httpd_req_t *req)
{
    static uint8_t buf[4096];
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1000)) != pdTRUE) goto busy;
    if (active) { xSemaphoreGive(lock); goto busy; }
    esp_err_t err = begin(req->content_len, "Wi-Fi");
    xSemaphoreGive(lock);
    size_t left = req->content_len;
    while (err == ESP_OK && left) {
        const int r = httpd_req_recv(req, (char *)buf, left < sizeof(buf) ? left : sizeof(buf));
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { err = ESP_FAIL; break; }
        xSemaphoreTake(lock, portMAX_DELAY);
        err = active ? esp_ota_write(handle, buf, r) : ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) { written += r; last_us = esp_timer_get_time(); }
        xSemaphoreGive(lock);
        left -= r;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    if (err == ESP_OK) err = finish();
    else if (active) { esp_ota_abort(handle); active = false; }
    xSemaphoreGive(lock);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        char text[80];
        snprintf(text, sizeof(text), "update failed: %s\n", esp_err_to_name(err));
        return httpd_resp_sendstr(req, text);
    }
    httpd_resp_sendstr(req, "OK: restarting into the new image\n");
    restart_soon();
    return ESP_OK;
busy:
    httpd_resp_set_status(req, "409 Conflict");
    return httpd_resp_sendstr(req, "busy: another update is running\n");
}

static void http_start(void)
{
    if (httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.recv_wait_timeout = 10;
    if (httpd_start(&httpd, &cfg) != ESP_OK) { httpd = NULL; return; }
    const httpd_uri_t info = { .uri = "/", .method = HTTP_GET, .handler = http_info };
    const httpd_uri_t ota = { .uri = "/ota", .method = HTTP_POST, .handler = http_ota };
    httpd_register_uri_handler(httpd, &info);
    httpd_register_uri_handler(httpd, &ota);
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi up: http://%s/ (update: POST the .bin to /ota)", ip_text);
        if (wifi_wanted) http_start();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ip_text[0] = 0;
        if (wifi_wanted) esp_wifi_connect();   // keep trying while it's wanted
    }
}

void ota_wifi_init(void)
{
    if (!lock) lock = xSemaphoreCreateMutex();
    esp_netif_init();
    esp_event_loop_create_default();
    sta = esp_netif_create_default_wifi_sta();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi, NULL);
}

bool ota_wifi_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("ota", NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = nvs_set_str(h, "ssid", ssid) == ESP_OK && nvs_set_str(h, "pass", pass) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

void ota_wifi_set(bool on)
{
    if (!sta) { ESP_LOGW(TAG, "no Wi-Fi on this board"); return; }
    if (!on) {
        wifi_wanted = false;
        if (httpd) { httpd_stop(httpd); httpd = NULL; }
        esp_wifi_disconnect();
        ip_text[0] = 0;
        ESP_LOGI(TAG, "Wi-Fi off");
        return;
    }
    wifi_config_t c = { 0 };
    nvs_handle_t h;
    size_t n1 = sizeof(c.sta.ssid), n2 = sizeof(c.sta.password);
    const bool have = nvs_open("ota", NVS_READONLY, &h) == ESP_OK &&
                      nvs_get_str(h, "ssid", (char *)c.sta.ssid, &n1) == ESP_OK &&
                      nvs_get_str(h, "pass", (char *)c.sta.password, &n2) == ESP_OK;
    if (have) nvs_close(h);
    if (!have || !c.sta.ssid[0]) { ESP_LOGW(TAG, "no network saved: \"wifi set SSID PASSWORD\" first"); return; }
    uint8_t primary;
    wifi_second_chan_t second;
    if (esp_wifi_get_channel(&primary, &second) == ESP_OK) c.sta.channel = primary;   // look on the radio's channel first
    c.sta.scan_method = WIFI_FAST_SCAN;
    esp_wifi_set_config(WIFI_IF_STA, &c);
    wifi_wanted = true;
    ESP_LOGI(TAG, "Wi-Fi joining \"%s\"", (char *)c.sta.ssid);
    esp_wifi_connect();
}

bool ota_wifi_on(void) { return wifi_wanted; }

void ota_wifi_text(char *out, size_t len)
{
    if (!wifi_wanted) out[0] = 0;
    else if (ip_text[0]) snprintf(out, len, "Update: http://%s/", ip_text);
    else snprintf(out, len, "Update: joining Wi-Fi...");
}
