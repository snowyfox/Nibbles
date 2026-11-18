// Updates asked for over the radio. See ota_remote.h.
#include "ota_remote.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nibbles_espnow.h"
#include "ota.h"

static const char *TAG = "ota_remote";

static uint8_t target;
static const char *my_name = "";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static char note[32];

void ota_remote_set_note(const char *n)
{
    taskENTER_CRITICAL(&lock);
    strlcpy(note, n ? n : "", sizeof(note));
    taskEXIT_CRITICAL(&lock);
}

bool ota_remote_radio(const nl_radio_hdr_t *hdr, const uint8_t *payload, size_t len)
{
    if (hdr->type == NL_MSG_OTA_STATE) return true;   // other boards': the base's business
    if (hdr->type != NL_MSG_OTA_WIFI) return false;
    if (len != sizeof(nl_ota_wifi_t) || hdr->role != NL_ROLE_BASE) return true;
    nl_ota_wifi_t m;
    memcpy(&m, payload, sizeof(m));
    if (!target || !nl_ota_wifi_for(&m, target)) return true;
    // Sent a few times: joining again is harmless (it only moves the time out).
    if (m.on && m.ssid[0]) {
        if (!ota_wifi_on()) ESP_LOGI(TAG, "the base asks for Wi-Fi: joining \"%s\" for %u min", m.ssid, m.minutes ? m.minutes : OTA_REMOTE_MINUTES);
        ota_wifi_join(m.ssid, m.pass, m.minutes ? m.minutes : OTA_REMOTE_MINUTES);
    } else if (!m.on && ota_wifi_on()) {
        ESP_LOGI(TAG, "the base asks for Wi-Fi off");
        ota_wifi_set(false);
    }
    return true;
}

static void state_task(void *arg)
{
    int tail = 0;   // a few more after Wi-Fi goes off, so the base sees it go
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        nl_ota_state_t s = { 0 };
        s.wifi = (uint8_t)ota_wifi_ip(s.ip);
        const int pct = ota_progress_pct();
        s.pct = pct < 0 ? NL_OTA_IDLE : (uint8_t)pct;
        taskENTER_CRITICAL(&lock);
        memcpy(s.note, note, sizeof(s.note));
        taskEXIT_CRITICAL(&lock);
        s.note[sizeof(s.note) - 1] = 0;
        const bool live = s.wifi != NL_OTA_WIFI_OFF || pct >= 0;
        if (live) tail = 3;
        else if (tail > 0) tail--;
        else continue;
        strlcpy(s.name, my_name, sizeof(s.name));
        strlcpy(s.build, ota_build_id(), sizeof(s.build));
        nl_espnow_broadcast(NL_MSG_OTA_STATE, &s, sizeof(s));
    }
}

void ota_remote_start(uint8_t my_target, const char *name)
{
    target = my_target;
    my_name = name;
    xTaskCreatePinnedToCore(state_task, "ota_state", 3072, NULL, 2, NULL, 0);
}

void ota_remote_text(char *out, size_t len)
{
    char p[48];
    ota_progress_text(p, sizeof(p));
    if (p[0]) { snprintf(out, len, "%s", p); return; }
    uint8_t ip[4];
    const int st = ota_wifi_ip(ip);
    if (st == NL_OTA_WIFI_UP) snprintf(out, len, "Update: http://%d.%d.%d.%d/", ip[0], ip[1], ip[2], ip[3]);
    else if (st == NL_OTA_WIFI_JOINING) snprintf(out, len, "Update: joining Wi-Fi...");
    else out[0] = 0;
}
