#include "nibbles_espnow.h"

#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_random.h"
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

static bool anchoring(void)
{
    return cfg.anchor;
}

// The site survey (nl_espnow_survey) has the radio: no channel changes or heartbeats meanwhile.
static volatile bool surveying;

// MACs heard sending Nibbles packets: their frames and access points (a
// brain's AP is its MAC + 1) are ours, left out of a survey.
#define KNOWN_MAX 24
static uint8_t known[KNOWN_MAX][6];
static int known_count;

static void remember(const uint8_t mac[6])
{
    for (int i = 0; i < known_count; i++)
        if (!memcmp(known[i], mac, 6)) return;
    if (known_count < KNOWN_MAX) memcpy(known[known_count++], mac, 6);
}

static bool ours(const uint8_t *mac)
{
    for (int i = 0; i < known_count; i++) {
        if (memcmp(known[i], mac, 5)) continue;
        const int d = (int)mac[5] - (int)known[i][5];
        if (d >= -1 && d <= 3) return true;   // the station MAC, its AP (+1) and neighbours
    }
    return false;
}

// A network move this node started: copies of the hop still to send.
static int move_copies;
static int64_t move_at_us, move_next_us;
static uint8_t move_channel;
static uint16_t move_id;

// Guards scan and the channel; the radio task and senders both move it.
static SemaphoreHandle_t chan_mutex;

static void set_channel(uint8_t ch)
{
    if (ch == channel) return;
    if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK) channel = ch;
}

// Before an application send (kept for the peeks fallback anchoring used to make).
static void about_to_send(void)
{
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
        .flags = anchoring() ? NL_HB_ANCHOR : scan.leading ? NL_HB_GROUP : 0,
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
    remember(it->mac);
    if (h.type == NL_MSG_HEARTBEAT && len == sizeof(nl_heartbeat_t)) {
        nl_heartbeat_t hb;
        memcpy(&hb, pl, sizeof(hb));
        if ((hb.flags & (NL_HB_ANCHOR | NL_HB_GROUP)) && !cfg.anchor && !surveying) {
            xSemaphoreTake(chan_mutex, portMAX_DELAY);
            if (hb.flags & NL_HB_ANCHOR) nl_chanscan_heard_anchor(&scan, hb.channel);
            else nl_chanscan_heard_group(&scan, it->mac, hb.channel);
            set_channel(scan.channel);
            xSemaphoreGive(chan_mutex);
        }
    } else if (h.type == NL_MSG_HOP && len == sizeof(nl_hop_t)) {
        nl_hop_t hop;
        memcpy(&hop, pl, sizeof(hop));
        if (!cfg.anchor && !surveying) {
            xSemaphoreTake(chan_mutex, portMAX_DELAY);
            nl_chanscan_heard_hop(&scan, it->mac, &hop);
            xSemaphoreGive(chan_mutex);
            if (hop.kind == NL_HOP_MOVE)
                ESP_LOGI(TAG, "network moves to channel %d in %d ms", hop.channel, hop.in_ms);
        }
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

static void send_hop(uint8_t kind, uint8_t ch, uint16_t in_ms, uint16_t id)
{
    const nl_hop_t hop = { .kind = kind, .channel = ch, .in_ms = in_ms, .id = id };
    send_raw(BROADCAST, NL_MSG_HOP, &hop, sizeof(hop));
}

static const char *scan_state(void)
{
    return scan.leading ? "leading a search" : scan.group ? "searching with a leader" : scan.locked ? "locked" : "scanning";
}

static void radio_task(void *arg)
{
    int64_t last = esp_timer_get_time(), next_hb = 0;
    const char *was = "";
    uint16_t search_id = 0;
    for (;;) {
        rx_item_t it;
        if (xQueueReceive(rx_queue, &it, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) handle(&it);
        const int64_t now = esp_timer_get_time();
        const uint32_t dt_ms = (uint32_t)((now - last) / 1000);
        if (surveying) {   // the survey has the radio; time it takes doesn't count against the lock
            last = now;
            continue;
        }
        if (dt_ms >= TICK_MS) {
            last = now;
            if (!cfg.anchor) {
                xSemaphoreTake(chan_mutex, portMAX_DELAY);
                const uint8_t before = channel;
                set_channel(nl_chanscan_tick(&scan, dt_ms));
                const bool announce = scan.announce;
                const uint8_t next = scan.announce_channel;
                scan.announce = false;
                xSemaphoreGive(chan_mutex);
                if (announce) {   // twice: a follower that misses both drops out and finds us again
                    search_id++;
                    send_hop(NL_HOP_SEARCH, next, NL_HOP_NOTICE_MS, search_id);
                    send_hop(NL_HOP_SEARCH, next, NL_HOP_NOTICE_MS - 10, search_id);
                }
                const char *st = scan_state();
                if (st != was) {
                    ESP_LOGI(TAG, "%s on channel %d", st, channel);
                    was = st;
                } else if (before != channel && scan.locked && !scan.group) {
                    ESP_LOGI(TAG, "moved to channel %d", channel);
                }
            }
        }
        if (move_copies > 0 && now >= move_next_us) {
            const int64_t left = (move_at_us - now) / 1000;
            if (left > 20) send_hop(NL_HOP_MOVE, move_channel, (uint16_t)left, move_id);
            move_copies = left > 20 ? move_copies - 1 : 0;
            move_next_us = now + 150 * 1000LL;
        }
        if (now >= next_hb) {
            next_hb = now + (anchoring() || scan.leading ? ANCHOR_HEARTBEAT_MS : HEARTBEAT_MS) * 1000LL;
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
    chan_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(rx_queue && cmd_mutex && ack_sem && chan_mutex, ESP_ERR_NO_MEM, TAG, "no memory");

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

    // Receivers treat a repeated (sender, id) as a retry and don't apply it
    // again, so start somewhere random: a rebooted sender counting from 1
    // would otherwise have its first command ignored.
    next_cmd_id = (uint16_t)(esp_random() | 1);
    nl_chanscan_init(&scan, cfg.anchor_channel);
    nl_chanscan_set_fallback(&scan, cfg.anchor ? 0 : cfg.anchor_fallback_ms);
    channel = 0;
    set_channel(cfg.anchor ? cfg.anchor_channel : scan.channel);

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    nl_chanscan_set_self(&scan, mac);
    remember(mac);
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x %s on channel %d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             cfg.anchor ? "anchoring" : "scanning, starting", channel);
    BaseType_t ok = xTaskCreatePinnedToCore(radio_task, "nl_radio", 4096, NULL, 5, NULL, cfg.core);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t nl_espnow_broadcast(uint8_t type, const void *payload, size_t len)
{
    about_to_send();
    return send_raw(BROADCAST, type, payload, len);
}

esp_err_t nl_espnow_send(const uint8_t mac[6], uint8_t type, const void *payload, size_t len)
{
    about_to_send();
    return send_raw(mac, type, payload, len);
}

esp_err_t nl_espnow_command(const uint8_t mac[6], nl_cmd_t *cmd, uint8_t *status, int tries, int timeout_ms)
{
    xSemaphoreTake(cmd_mutex, portMAX_DELAY);
    cmd->id = next_cmd_id++;
    if (!next_cmd_id) next_cmd_id = 1;
    xSemaphoreTake(ack_sem, 0);  // clear a stale ack
    about_to_send();
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
    out->locked = anchoring() || (scan.locked && !scan.group);
    out->anchoring = anchoring();
    out->leading = scan.leading;
    out->group = scan.group;
    out->rx = rx_count;
    out->tx = tx_count;
    out->tx_fail = tx_fail;
    out->locks = scan.locks;
    out->dropped = dropped;
    taskEXIT_CRITICAL(&lock);
}

esp_err_t nl_espnow_move(uint8_t ch, uint32_t in_ms)
{
    if (cfg.anchor) return ESP_ERR_NOT_SUPPORTED;
    xSemaphoreTake(chan_mutex, portMAX_DELAY);
    nl_chanscan_move(&scan, ch, in_ms);
    const bool ok = scan.hop_pending && scan.hop_kind == NL_HOP_MOVE && scan.hop_channel == ch;
    xSemaphoreGive(chan_mutex);
    if (!ok) return ESP_ERR_INVALID_STATE;   // no anchor to move with, or already there
    const int64_t now = esp_timer_get_time();
    move_channel = ch;
    move_id = (uint16_t)esp_random();
    move_at_us = now + in_ms * 1000LL;
    move_next_us = now;
    move_copies = 5;
    ESP_LOGI(TAG, "moving the network to channel %d in %lu ms", ch, (unsigned long)in_ms);
    return ESP_OK;
}

// ---------------------------------------------------------------- site survey

typedef struct {
    uint32_t frames, air_us;
} survey_acc_t;
static survey_acc_t acc;

// How long a received frame held the air (µs), from its rate and length.
static uint32_t frame_us(const wifi_pkt_rx_ctrl_t *rx)
{
    const uint32_t bits = rx->sig_len * 8u;
    if (rx->sig_mode == 0) {
        // wifi_phy_rate_t: 802.11b (long/short preamble), then 802.11g OFDM.
        static const uint16_t kbps[16] = { 1000, 2000, 5500, 11000, 0, 2000, 5500, 11000,
                                           48000, 24000, 12000, 6000, 54000, 36000, 18000, 9000 };
        const unsigned r = rx->rate & 0x0F;
        const uint32_t k = kbps[r] ? kbps[r] : 1000;
        const uint32_t pre = r < 4 ? 192 : r < 8 ? 96 : 20;
        return pre + bits * 1000u / k;
    }
    // 802.11n, 20 MHz, long guard interval; two streams from MCS 8.
    static const uint16_t ht_kbps[8] = { 6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000 };
    uint32_t k = ht_kbps[rx->mcs & 7] * (rx->mcs >= 8 ? 2u : 1u);
    if (rx->cwb) k = k * 208 / 100;
    return 36 + bits * 1000u / k;
}

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    const wifi_promiscuous_pkt_t *p = buf;
    const uint8_t *f = p->payload;
    const unsigned len = p->rx_ctrl.sig_len;
    if (len >= 10 && ours(f + 4)) return;                                   // to one of ours
    if (len >= 16 && type != WIFI_PKT_CTRL && ours(f + 10)) return;          // from one of ours
    acc.frames++;
    acc.air_us += frame_us(&p->rx_ctrl);
}

esp_err_t nl_espnow_survey(nl_survey_t *out, uint32_t dwell_ms)
{
    if (surveying) return ESP_ERR_INVALID_STATE;
    memset(out, 0, sizeof(*out));
    for (int c = 0; c <= NL_CHANNEL_MAX; c++) out->ch[c].strongest = -127;
    xSemaphoreTake(chan_mutex, portMAX_DELAY);
    out->current = channel;
    surveying = true;
    xSemaphoreGive(chan_mutex);
    vTaskDelay(pdMS_TO_TICKS(TICK_MS * 2));   // the radio task sees it

    // Traffic: listen to every channel in turn.
    const wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA |
                                                            WIFI_PROMIS_FILTER_MASK_CTRL };
    const wifi_promiscuous_filter_t ctrl = { .filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL };
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_ctrl_filter(&ctrl);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    for (int c = NL_CHANNEL_MIN; c <= NL_CHANNEL_MAX; c++) {
        esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
        memset(&acc, 0, sizeof(acc));
        esp_wifi_set_promiscuous(true);
        vTaskDelay(pdMS_TO_TICKS(dwell_ms));
        esp_wifi_set_promiscuous(false);
        const uint32_t frames = acc.frames, air = acc.air_us;
        out->ch[c].frames = (uint16_t)(frames * 1000u / dwell_ms > 65535 ? 65535 : frames * 1000u / dwell_ms);
        const uint32_t pml = air / dwell_ms;   // µs per ms = per mille
        out->ch[c].busy_pml = (uint16_t)(pml > 1000 ? 1000 : pml);
    }

    // Access points: an active scan of every channel.
    const wifi_scan_config_t sc = { .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                                    .scan_time.active = { .min = 60, .max = 120 } };
    if (esp_wifi_scan_start(&sc, true) == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        if (n > 64) n = 64;
        wifi_ap_record_t *aps = n ? calloc(n, sizeof(*aps)) : NULL;
        if (aps && esp_wifi_scan_get_ap_records(&n, aps) == ESP_OK) {
            for (int i = 0; i < n; i++) {
                const int c = aps[i].primary;
                if (c < NL_CHANNEL_MIN || c > NL_CHANNEL_MAX || ours(aps[i].bssid)) continue;
                nl_survey_chan_t *ch = &out->ch[c];
                if (ch->aps < 255) ch->aps++;
                if (aps[i].rssi > ch->strongest) ch->strongest = aps[i].rssi;
                if (aps[i].rssi > -95) ch->ap_weight += aps[i].rssi + 95;
            }
        } else {
            esp_wifi_clear_ap_list();
        }
        free(aps);
    }

    // Back where we were.
    xSemaphoreTake(chan_mutex, portMAX_DELAY);
    channel = 0;
    set_channel(scan.channel);
    scan.since_heard_ms = 0;   // give the anchor a moment to be heard again
    surveying = false;
    xSemaphoreGive(chan_mutex);
    return ESP_OK;
}
