// Wired link between the two eyes: UART1 on header GPIO 17 (TX) and 18 (RX),
// crossed between the boards, carrying nibbles_link frames.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nibbles_link.h"

esp_err_t link_start(void);

// True while heartbeats arrive from the other eye (not our own echo).
bool link_peer_up(void);

// Latest message of each kind from the other eye, if newer than max_age_ms.
bool link_get_audio(nl_audio_t *out, uint32_t max_age_ms);
bool link_get_eye_state(nl_eye_state_t *out, uint32_t max_age_ms);

void link_send_audio(const nl_audio_t *a);
void link_send_eye_state(const nl_eye_state_t *s);

// Figures reported in our heartbeat.
void link_set_status(uint16_t fps_x10, uint16_t late_frames);

typedef struct {
    uint32_t rx_frames, tx_frames;  // since boot
    uint32_t crc_errors, bad_frames;
    uint32_t own_frames;            // our own heartbeats heard back (a loopback jumper)
    bool peer_up;
    uint16_t peer_fps_x10;
    uint16_t peer_late;             // the other eye's late frames in its last second
} link_stats_t;

void link_get_stats(link_stats_t *out);
