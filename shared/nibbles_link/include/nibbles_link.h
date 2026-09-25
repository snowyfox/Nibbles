// Nibbles link protocol: the messages the shark's boards exchange, and the
// framing used on byte streams (the wired eye link). Pure C, no platform
// dependencies, so it builds for ESP-IDF, WLED (Arduino/C++) and host tests.
//
// Frame on the wire: COBS( type | version | seq (LE16) | payload | CRC-16 (LE) ) 0x00
// Payload structs are packed and little-endian (every target is).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NL_VERSION          1
#define NL_MAX_PAYLOAD      128
#define NL_HEADER_LEN       4   // type, version, seq
#define NL_CRC_LEN          2
#define NL_MAX_RAW          (NL_HEADER_LEN + NL_MAX_PAYLOAD + NL_CRC_LEN)
#define NL_MAX_FRAME        (NL_MAX_RAW + NL_MAX_RAW / 254 + 2)  // COBS overhead + delimiter

typedef enum {
    NL_MSG_HEARTBEAT = 1,
    NL_MSG_AUDIO = 2,       // audio features ("the system's ears")
    NL_MSG_EYE_STATE = 3,   // shared eye behaviour, leader eye -> other eye
} nl_msg_type_t;

typedef enum {
    NL_ROLE_NONE = 0,
    NL_ROLE_EYE_LEADER = 1, // runs the shared eye brain; the eyes' radio gateway
    NL_ROLE_EYE_EARS = 2,   // runs the mic and sends audio features
    NL_ROLE_BASE = 3,
    NL_ROLE_WLED = 4,
} nl_role_t;

typedef enum {
    NL_SIDE_NONE = 0,
    NL_SIDE_STARBOARD = 1,
    NL_SIDE_PORT = 2,
} nl_side_t;

#define NL_PACKED __attribute__((packed))

typedef struct NL_PACKED {
    uint8_t role;           // nl_role_t
    uint8_t side;           // nl_side_t
    uint16_t fw;            // firmware build number
    uint32_t uptime_ms;
    uint16_t fps_x10;
    uint16_t late_frames;   // since the last heartbeat
} nl_heartbeat_t;

typedef struct NL_PACKED {
    float level_db;         // RMS level with the mic gain removed
    float avg_db;           // ~1.5 s average level
    float noise_floor_db;
    float gain_db;          // mic gain in use
    float loudness;         // 0..1
    float warmth;           // 0 treble .. 1 bass
    float beat_period_s;    // 0 when there is no steady tempo
    float beat_confidence;  // rhythm strength 0..1
    uint32_t beat_count;    // increments on every beat
} nl_audio_t;

#define NL_RIPPLES 4

typedef struct NL_PACKED {
    float time_s;           // the leader's animation clock
    float hue, intensity, lid_open, hype, ring_phase, tempo_bpm, wobble, pupil_r;
    float gaze_x, gaze_y;   // idle glance, -1..1 in the sender's screen frame
    float ripple_r[NL_RIPPLES], ripple_amp[NL_RIPPLES];
    uint8_t state;          // eye state (awake, drowsy, asleep, waking)
    uint8_t preset;         // current preset index
    uint8_t awake;
    uint8_t side;           // sender's side (nl_side_t), to mirror the glance
} nl_eye_state_t;

// Encode one frame into out (at most NL_MAX_FRAME bytes, including the
// trailing 0x00). Returns the number of bytes, or 0 if it doesn't fit.
size_t nl_encode(uint8_t type, uint16_t seq, const void *payload, size_t len, uint8_t *out, size_t cap);

typedef struct {
    uint8_t type, version;
    uint16_t seq;
    const uint8_t *payload; // valid until the next nl_parse_byte call
    size_t len;
} nl_frame_t;

typedef struct {
    uint8_t buf[NL_MAX_FRAME];
    size_t n;
    bool overflow;
    uint8_t dec[NL_MAX_RAW];
    uint32_t frames, crc_errors, bad_frames;  // counters for link stats
} nl_parser_t;

void nl_parser_init(nl_parser_t *p);

// Feed one received byte. Returns true when a complete, valid frame has been
// decoded into *out. Corrupt or oversized frames are counted and dropped;
// the parser resynchronises on the next 0x00.
bool nl_parse_byte(nl_parser_t *p, uint8_t byte, nl_frame_t *out);

// CRC-16/CCITT-FALSE, exposed for tests.
uint16_t nl_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
