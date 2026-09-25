// ESP-NOW transport for Nibbles boards built with ESP-IDF (the eyes and the
// base station). WLED uses its own ESP-NOW and only the nibbles_link packets.
//
// Wi-Fi runs in station mode without connecting, only to carry ESP-NOW. A
// node is either the channel anchor (stays on a fixed channel and broadcasts
// anchor heartbeats) or a follower that scans for the anchor (see
// nl_chanscan_* in nibbles_link.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nibbles_link.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called on the radio task for every valid Nibbles packet (acks included).
typedef void (*nl_espnow_handler_t)(const uint8_t mac[6], const nl_radio_hdr_t *hdr, const uint8_t *payload, size_t len);

typedef struct {
    nl_role_t role;
    nl_side_t side;
    uint16_t fw;
    bool anchor;             // true: stay on anchor_channel and broadcast anchor heartbeats
    uint8_t anchor_channel;  // anchor's channel; followers start scanning here
    nl_espnow_handler_t handler;
    int core;                // core for the radio task
} nl_espnow_config_t;

// Needs NVS initialised first.
esp_err_t nl_espnow_start(const nl_espnow_config_t *cfg);

esp_err_t nl_espnow_broadcast(uint8_t type, const void *payload, size_t len);
esp_err_t nl_espnow_send(const uint8_t mac[6], uint8_t type, const void *payload, size_t len);

// Send a command and wait for its ack, retrying with the same id so it is
// applied once. Fills cmd->id. Blocks the caller for up to tries * timeout_ms.
esp_err_t nl_espnow_command(const uint8_t mac[6], nl_cmd_t *cmd, uint8_t *ack_status, int tries, int timeout_ms);

// Answer a received command.
void nl_espnow_ack(const uint8_t mac[6], uint16_t id, uint8_t status);

// Figures reported in this node's heartbeat.
void nl_espnow_set_status(uint16_t fps_x10, uint16_t late_frames);

typedef struct {
    uint8_t channel;
    bool locked;             // hearing the anchor (always true on the anchor itself)
    uint32_t rx, tx, tx_fail, locks, dropped;
} nl_espnow_stats_t;

void nl_espnow_get_stats(nl_espnow_stats_t *out);

#ifdef __cplusplus
}
#endif
