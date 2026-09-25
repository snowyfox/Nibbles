// Wired eye link task.
#include "link.h"

#include <string.h>
#include "board.h"
#include "config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "link";

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static nl_parser_t parser;
static uint16_t tx_seq;
static nl_audio_t rx_audio;
static nl_eye_state_t rx_eye;
static nl_heartbeat_t rx_heartbeat;
static int64_t rx_audio_us, rx_eye_us, peer_us;
static uint32_t tx_frames, own_frames;
static uint16_t my_fps_x10, my_late;

static void send(uint8_t type, const void *payload, size_t len)
{
    uint8_t frame[NL_MAX_FRAME];
    taskENTER_CRITICAL(&lock);
    const uint16_t seq = tx_seq++;
    taskEXIT_CRITICAL(&lock);
    const size_t n = nl_encode(type, seq, payload, len, frame, sizeof(frame));
    if (n && uart_write_bytes(LINK_UART, frame, n) == (int)n) {
        taskENTER_CRITICAL(&lock);
        tx_frames++;
        taskEXIT_CRITICAL(&lock);
    }
}

static void send_heartbeat(void)
{
    nl_heartbeat_t hb = {
        .role = board_role(),
        .side = board_side(),
        .fw = NIBBLES_FW_BUILD,
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000),
    };
    taskENTER_CRITICAL(&lock);
    hb.fps_x10 = my_fps_x10;
    hb.late_frames = my_late;
    taskEXIT_CRITICAL(&lock);
    send(NL_MSG_HEARTBEAT, &hb, sizeof(hb));
}

// Each eye only uses the other role's messages, so its own frames heard back
// through a loopback jumper are ignored (heartbeats are counted to show it).
static void handle(const nl_frame_t *f)
{
    if (f->version != NL_VERSION) return;
    const int64_t now = esp_timer_get_time();
    const nl_role_t me = board_role();
    taskENTER_CRITICAL(&lock);
    if (f->type == NL_MSG_HEARTBEAT && f->len == sizeof(nl_heartbeat_t)) {
        nl_heartbeat_t hb;
        memcpy(&hb, f->payload, sizeof(hb));
        if (hb.role == me && hb.side == board_side()) {
            own_frames++;
        } else {
            rx_heartbeat = hb;
            peer_us = now;
        }
    } else if (f->type == NL_MSG_AUDIO && f->len == sizeof(nl_audio_t) && me == NL_ROLE_EYE_LEADER) {
        memcpy(&rx_audio, f->payload, sizeof(rx_audio));
        rx_audio_us = now;
    } else if (f->type == NL_MSG_EYE_STATE && f->len == sizeof(nl_eye_state_t) && me == NL_ROLE_EYE_EARS) {
        memcpy(&rx_eye, f->payload, sizeof(rx_eye));
        rx_eye_us = now;
    }
    taskEXIT_CRITICAL(&lock);
}

static void link_task(void *arg)
{
    uint8_t buf[256];
    int64_t next_heartbeat = 0;
    for (;;) {
        const int n = uart_read_bytes(LINK_UART, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; i++) {
            nl_frame_t f;
            if (nl_parse_byte(&parser, buf[i], &f)) handle(&f);
        }
        const int64_t now = esp_timer_get_time();
        if (now >= next_heartbeat) {
            next_heartbeat = now + 1000000 / LINK_HEARTBEAT_HZ;
            send_heartbeat();
        }
    }
}

esp_err_t link_start(void)
{
    nl_parser_init(&parser);
    const uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(LINK_UART, 2048, 2048, 0, NULL, 0), TAG, "uart install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(LINK_UART, &cfg), TAG, "uart config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(LINK_UART, LINK_TX_GPIO, LINK_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart pins failed");
    gpio_set_pull_mode(LINK_RX_GPIO, GPIO_PULLUP_ONLY);  // idle-high when the cable is unplugged
    BaseType_t ok = xTaskCreatePinnedToCore(link_task, "link", 4096, NULL, 5, NULL, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");
    ESP_LOGI(TAG, "%s eye, role %s, UART%d TX GPIO%d RX GPIO%d at %d baud",
             board_side() == NL_SIDE_PORT ? "port" : "starboard",
             board_role() == NL_ROLE_EYE_LEADER ? "leader" : "ears", LINK_UART, LINK_TX_GPIO, LINK_RX_GPIO, LINK_BAUD);
    return ESP_OK;
}

static bool fresh(int64_t t, uint32_t max_age_ms)
{
    return t != 0 && esp_timer_get_time() - t <= (int64_t)max_age_ms * 1000;
}

bool link_peer_up(void)
{
    taskENTER_CRITICAL(&lock);
    const bool up = fresh(peer_us, LINK_PEER_TIMEOUT_MS);
    taskEXIT_CRITICAL(&lock);
    return up;
}

bool link_get_audio(nl_audio_t *out, uint32_t max_age_ms)
{
    taskENTER_CRITICAL(&lock);
    const bool ok = fresh(rx_audio_us, max_age_ms);
    if (ok) *out = rx_audio;
    taskEXIT_CRITICAL(&lock);
    return ok;
}

bool link_get_eye_state(nl_eye_state_t *out, uint32_t max_age_ms)
{
    taskENTER_CRITICAL(&lock);
    const bool ok = fresh(rx_eye_us, max_age_ms);
    if (ok) *out = rx_eye;
    taskEXIT_CRITICAL(&lock);
    return ok;
}

void link_send_audio(const nl_audio_t *a) { send(NL_MSG_AUDIO, a, sizeof(*a)); }

void link_send_eye_state(const nl_eye_state_t *s) { send(NL_MSG_EYE_STATE, s, sizeof(*s)); }

void link_set_status(uint16_t fps_x10, uint16_t late_frames)
{
    taskENTER_CRITICAL(&lock);
    my_fps_x10 = fps_x10;
    my_late = late_frames;
    taskEXIT_CRITICAL(&lock);
}

void link_get_stats(link_stats_t *out)
{
    taskENTER_CRITICAL(&lock);
    out->rx_frames = parser.frames;
    out->crc_errors = parser.crc_errors;
    out->bad_frames = parser.bad_frames;
    out->tx_frames = tx_frames;
    out->own_frames = own_frames;
    out->peer_up = fresh(peer_us, LINK_PEER_TIMEOUT_MS);
    out->peer_fps_x10 = rx_heartbeat.fps_x10;
    out->peer_late = rx_heartbeat.late_frames;
    taskEXIT_CRITICAL(&lock);
}
