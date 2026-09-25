#include "nibbles_espnow.h"

#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "espnow";

#define ANCHOR_HEARTBEAT_MS  250   // 4 Hz, so a scanning node hears one per dwell
#define HEARTBEAT_MS         500
#define TICK_MS              50

typedef struct {
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[NL_RADIO_MAX];
} rx_item_t;

static const uint8_t BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static nl_espnow_config_t cfg;
static QueueHandle_t rx_queue;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static nl_chanscan_t scan;
static uint8_t channel;
static uint16_t seq, next_cmd_id = 1;
static uint16_t my_fps_x10, my_late;
static uint32_t rx_count, tx_count, tx_fail, dropped;

// One command in flight at a time.
static SemaphoreHandle_t cmd_mutex, ack_sem;
static volatile uint16_t waiting_id;
static volatile uint8_t ack_status;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len <= 0 || len > NL_RADIO_MAX) return;
    rx_item_t item;
    memcpy(item.mac, info->src_addr, 6);
    item.len = (uint8_t)len;
    memcpy(item.data, data, len);
    if (xQueueSend(rx_queue, &item, 0) != pdTRUE) {
        taskENTER_CRITICAL(&lock);
        dropped++;
        taskEXIT_CRITICAL(&lock);
    }
}

static void on_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    (void)info;
    taskENTER_CRITICAL(&lock);
    if (status == ESP_NOW_SEND_SUCCESS) tx_count++;
    else tx_fail++;
    taskEXIT_CRITICAL(&lock);
}

static void set_channel(uint8_t ch)
{
    if (ch == channel) return;
    if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK) channel = ch;
}

static esp_err_t ensure_peer(const uint8_t mac[6])
{
    if (esp_now_is_peer_exist(mac)) return ESP_OK;
    esp_now_peer_info_t peer = { .channel = 0, .ifidx = WIFI_IF_STA, .encrypt = false };  // 0 = current channel
    memcpy(peer.peer_addr, mac, 6);
    return esp_now_add_peer(&peer);
}

static esp_err_t send_raw(const uint8_t mac[6], uint8_t type, const void *payload, size_t len)
{
    uint8_t pkt[NL_RADIO_MAX];
    taskENTER_CRITICAL(&lock);
    const uint16_t s = seq++;
    taskEXIT_CRITICAL(&lock);
    const size_t n = nl_radio_build(pkt, sizeof(pkt), type, s, cfg.role, cfg.side, payload, len);
    if (!n) return ESP_ERR_INVALID_SIZE;
    ESP_RETURN_ON_ERROR(ensure_peer(mac), TAG, "add peer failed");
    return esp_now_send(mac, pkt, n);
}

static void send_heartbeat(void)
{
    nl_heartbeat_t hb = {
        .role = cfg.role,
        .side = cfg.side,
        .fw = cfg.fw,
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .flags = cfg.anchor ? NL_HB_ANCHOR : 0,
        .channel = channel,
    };
    taskENTER_CRITICAL(&lock);
    hb.fps_x10 = my_fps_x10;
    hb.late_frames = my_late;
    taskEXIT_CRITICAL(&lock);
    send_raw(BROADCAST, NL_MSG_HEARTBEAT, &hb, sizeof(hb));
}

static void handle(const rx_item_t *it)
{
    nl_radio_hdr_t h;
    const uint8_t *pl;
    size_t len;
    if (!nl_radio_parse(it->data, it->len, &h, &pl, &len)) return;
    taskENTER_CRITICAL(&lock);
    rx_count++;
    taskEXIT_CRITICAL(&lock);
    if (h.type == NL_MSG_HEARTBEAT && len == sizeof(nl_heartbeat_t)) {
        nl_heartbeat_t hb;
        memcpy(&hb, pl, sizeof(hb));
        if ((hb.flags & NL_HB_ANCHOR) && !cfg.anchor) nl_chanscan_heard_anchor(&scan);
    } else if (h.type == NL_MSG_ACK && len == sizeof(nl_ack_t)) {
        nl_ack_t ack;
        memcpy(&ack, pl, sizeof(ack));
        if (waiting_id && ack.id == waiting_id) {
            ack_status = ack.status;
            waiting_id = 0;
            xSemaphoreGive(ack_sem);
        }
    }
    if (cfg.handler) cfg.handler(it->mac, &h, pl, len);
}

static void radio_task(void *arg)
{
    int64_t last = esp_timer_get_time(), next_hb = 0;
    for (;;) {
        rx_item_t it;
        if (xQueueReceive(rx_queue, &it, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) handle(&it);
        const int64_t now = esp_timer_get_time();
        const uint32_t dt_ms = (uint32_t)((now - last) / 1000);
        if (dt_ms >= TICK_MS) {
            last = now;
            if (!cfg.anchor) set_channel(nl_chanscan_tick(&scan, dt_ms));
        }
        if (now >= next_hb) {
            next_hb = now + (cfg.anchor ? ANCHOR_HEARTBEAT_MS : HEARTBEAT_MS) * 1000LL;
            send_heartbeat();
        }
    }
}

esp_err_t nl_espnow_start(const nl_espnow_config_t *c)
{
    cfg = *c;
    rx_queue = xQueueCreate(16, sizeof(rx_item_t));
    cmd_mutex = xSemaphoreCreateMutex();
    ack_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(rx_queue && cmd_mutex && ack_sem, ESP_ERR_NO_MEM, TAG, "no memory");

    esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop failed");
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wcfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    esp_wifi_set_ps(WIFI_PS_NONE);  // keep listening; no router to save power against
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp-now init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(on_recv), TAG, "recv cb failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(on_sent), TAG, "send cb failed");
    ESP_RETURN_ON_ERROR(ensure_peer(BROADCAST), TAG, "broadcast peer failed");

    nl_chanscan_init(&scan, cfg.anchor_channel);
    channel = 0;
    set_channel(cfg.anchor ? cfg.anchor_channel : scan.channel);

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x %s on channel %d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             cfg.anchor ? "anchoring" : "scanning, starting", channel);
    BaseType_t ok = xTaskCreatePinnedToCore(radio_task, "nl_radio", 4096, NULL, 5, NULL, cfg.core);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t nl_espnow_broadcast(uint8_t type, const void *payload, size_t len)
{
    return send_raw(BROADCAST, type, payload, len);
}

esp_err_t nl_espnow_send(const uint8_t mac[6], uint8_t type, const void *payload, size_t len)
{
    return send_raw(mac, type, payload, len);
}

esp_err_t nl_espnow_command(const uint8_t mac[6], nl_cmd_t *cmd, uint8_t *status, int tries, int timeout_ms)
{
    xSemaphoreTake(cmd_mutex, portMAX_DELAY);
    cmd->id = next_cmd_id++;
    if (!next_cmd_id) next_cmd_id = 1;
    xSemaphoreTake(ack_sem, 0);  // clear a stale ack
    esp_err_t result = ESP_ERR_TIMEOUT;
    for (int i = 0; i < tries && result != ESP_OK; i++) {
        waiting_id = cmd->id;
        if (send_raw(mac, NL_MSG_CMD, cmd, sizeof(*cmd)) != ESP_OK) continue;
        if (xSemaphoreTake(ack_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
            if (status) *status = ack_status;
            result = ESP_OK;
        }
    }
    waiting_id = 0;
    xSemaphoreGive(cmd_mutex);
    return result;
}

void nl_espnow_ack(const uint8_t mac[6], uint16_t id, uint8_t status)
{
    const nl_ack_t ack = { .id = id, .status = status };
    send_raw(mac, NL_MSG_ACK, &ack, sizeof(ack));
}

void nl_espnow_set_status(uint16_t fps_x10, uint16_t late_frames)
{
    taskENTER_CRITICAL(&lock);
    my_fps_x10 = fps_x10;
    my_late = late_frames;
    taskEXIT_CRITICAL(&lock);
}

void nl_espnow_get_stats(nl_espnow_stats_t *out)
{
    taskENTER_CRITICAL(&lock);
    out->channel = channel;
    out->locked = cfg.anchor || scan.locked;
    out->rx = rx_count;
    out->tx = tx_count;
    out->tx_fail = tx_fail;
    out->locks = scan.locks;
    out->dropped = dropped;
    taskEXIT_CRITICAL(&lock);
}
