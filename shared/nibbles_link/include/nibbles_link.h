// Nibbles link protocol: the messages the shark's boards exchange, and the
// framing used on byte streams (the wired eye link). Pure C, no platform
// dependencies, so it builds for ESP-IDF, WLED (Arduino/C++) and host tests.
//
// Wired frame: COBS( type | version | seq (LE16) | payload | CRC-16 (LE) ) 0x00
// Radio packet (ESP-NOW, which has its own CRC): nl_radio_hdr_t | payload
// Payload structs are packed and little-endian (every target is).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NL_VERSION          3
#define NL_MAX_PAYLOAD      128
#define NL_HEADER_LEN       4   // type, version, seq
#define NL_CRC_LEN          2
#define NL_MAX_RAW          (NL_HEADER_LEN + NL_MAX_PAYLOAD + NL_CRC_LEN)
#define NL_MAX_FRAME        (NL_MAX_RAW + NL_MAX_RAW / 254 + 2)  // COBS overhead + delimiter

typedef enum {
    NL_MSG_HEARTBEAT = 1,
    NL_MSG_AUDIO = 2,       // audio features ("the system's ears")
    NL_MSG_EYE_STATE = 3,   // shared eye behaviour, leader eye -> other eye
    NL_MSG_EYE_TELEMETRY = 4, // eye status, leader eye -> radio, 2 Hz
    NL_MSG_CMD = 5,         // a command, unicast, answered with NL_MSG_ACK
    NL_MSG_ACK = 6,
    NL_MSG_WLED_TELEMETRY = 7, // WLED status, WLED -> radio, 2 Hz
    NL_MSG_BUMP = 8,        // momentary effect while a button is held (broadcast)
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

#define NL_HB_ANCHOR 0x01   // the sender sets the radio channel everyone else follows

typedef struct NL_PACKED {
    uint8_t role;           // nl_role_t
    uint8_t side;           // nl_side_t
    uint16_t fw;            // firmware build number
    uint32_t uptime_ms;
    uint16_t fps_x10;
    uint16_t late_frames;   // since the last heartbeat
    uint8_t flags;          // NL_HB_*
    uint8_t channel;        // radio channel the sender is on (0 = none)
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
#define NL_NAME_LEN 20          // preset names in telemetry, terminator included

typedef struct NL_PACKED {
    float time_s;           // the leader's animation clock
    float hue, intensity, lid_open, hype, ring_phase, tempo_bpm, wobble, pupil_r;
    float gaze_x, gaze_y;   // idle glance, -1..1 in the sender's screen frame
    float ripple_r[NL_RIPPLES], ripple_amp[NL_RIPPLES];
    uint8_t state;          // eye state (awake, drowsy, asleep, waking)
    uint8_t preset;         // current preset index
    uint8_t awake;
    uint8_t side;           // sender's side (nl_side_t), to mirror the glance
    uint8_t master;         // master level for everything, outline included (255 = normal, 0 = blackout)
    uint8_t brightness;     // panel brightness in percent; the other eye matches it
} nl_eye_state_t;

typedef struct NL_PACKED {
    uint8_t preset;         // current preset index
    uint8_t preset_count;
    uint8_t state;          // eye state (awake, drowsy, asleep, waking)
    uint8_t linked;         // the two eyes are linked by the cable
    uint16_t fps_x10[2];    // [0] this (leader) eye, [1] the other eye
    uint16_t late_frames[2];// since the last telemetry
    float tempo_bpm;        // 0 = no steady beat
    float level_db;
    float hype;
    uint8_t brightness;     // panel brightness in percent
    char name[NL_NAME_LEN]; // current preset's name, NUL-terminated
    uint8_t auto_cycle;     // 1 = presets change by themselves
} nl_eye_telemetry_t;

typedef struct NL_PACKED {
    uint8_t on;             // lights on
    uint8_t bri;            // master brightness 0..255
    uint8_t preset;         // current preset id (0 = none)
    uint8_t fx;             // main segment effect
    uint8_t palette;        // main segment palette
    uint8_t channel;        // radio channel
    uint16_t fps;
    uint16_t leds;          // total LED count
    char name[NL_NAME_LEN]; // current preset's name, NUL-terminated ("" = none)
} nl_wled_telemetry_t;

typedef enum {
    NL_TARGET_EYES = 1,
    NL_TARGET_WLED = 2,
} nl_target_t;

typedef enum {
    NL_OP_PRESET_SET = 1,   // arg = preset index
    NL_OP_PRESET_NEXT = 2,
    NL_OP_PRESET_PREV = 3,
    NL_OP_BRIGHTNESS_SET = 4,   // arg = 0..255
    NL_OP_BRIGHTNESS_STEP = 5,  // arg = signed step (eyes: through their brightness presets by sign)
    NL_OP_AUTO_CYCLE = 6,       // eyes: arg 1 = cycle presets by themselves, 0 = hold the current one
} nl_op_t;

typedef struct NL_PACKED {
    uint16_t id;            // echoed in the ack; retries reuse it so a command applies once
    uint8_t target;         // nl_target_t
    uint8_t op;             // nl_op_t
    int16_t arg;
} nl_cmd_t;

typedef enum {
    NL_ACK_OK = 0,
    NL_ACK_UNSUPPORTED = 1,
    NL_ACK_BAD_ARG = 2,
} nl_ack_status_t;

typedef struct NL_PACKED {
    uint16_t id;
    uint8_t status;         // nl_ack_status_t
} nl_ack_t;

// Bumps: DMX-style momentary effects, active only while a button is held.
// The sender broadcasts START, then HOLD every NL_BUMP_KEEPALIVE_MS, then
// STOP. A receiver treats HOLD for an unknown bump as a start (START lost)
// and releases a bump that goes NL_BUMP_TIMEOUT_MS without a message (STOP
// lost), so an effect can never stick on.
#define NL_BUMP_KEEPALIVE_MS 100
#define NL_BUMP_TIMEOUT_MS   300

typedef enum {
    NL_BUMP_FLASH = 1,      // everything white, full brightness
    NL_BUMP_BLACKOUT = 2,   // everything off
    NL_BUMP_PRESET = 3,     // arg = preset to show while held
} nl_bump_action_t;

typedef enum {
    NL_BUMP_START = 1,
    NL_BUMP_HOLD = 2,
    NL_BUMP_STOP = 3,
} nl_bump_phase_t;

typedef struct NL_PACKED {
    uint16_t id;            // one id per press; all messages of a press share it
    uint8_t target;         // nl_target_t (bit mask: NL_TARGET_EYES | NL_TARGET_WLED)
    uint8_t action;         // nl_bump_action_t
    uint8_t phase;          // nl_bump_phase_t
    uint8_t intensity;      // 0..255
    int16_t arg;
} nl_bump_t;

// Receiver side: which bump (if any) is active. One active bump at a time;
// a new press replaces the current one.
typedef struct {
    bool active;
    nl_bump_t bump;
    uint32_t last_ms;
} nl_bump_rx_t;

typedef enum {
    NL_BUMP_EV_NONE = 0,
    NL_BUMP_EV_BEGIN,       // apply the effect in rx->bump
    NL_BUMP_EV_END,         // restore
    NL_BUMP_EV_REPLACE,     // restore, then apply the new rx->bump
} nl_bump_event_t;

void nl_bump_rx_init(nl_bump_rx_t *rx);
// A bump message arrived at now_ms.
nl_bump_event_t nl_bump_rx_message(nl_bump_rx_t *rx, const nl_bump_t *b, uint32_t now_ms);
// Call regularly; returns NL_BUMP_EV_END when the active bump times out.
nl_bump_event_t nl_bump_rx_tick(nl_bump_rx_t *rx, uint32_t now_ms);

// ---------------------------------------------------------------- radio packets
#define NL_NET_ID           0x5348  // "SH"(ark): packets from other networks are ignored
#define NL_RADIO_MAGIC0     'N'
#define NL_RADIO_MAGIC1     'B'

typedef struct NL_PACKED {
    uint8_t magic[2];
    uint16_t net_id;
    uint8_t type, version;
    uint16_t seq;
    uint8_t role, side;     // sender
} nl_radio_hdr_t;

#define NL_RADIO_MAX        (sizeof(nl_radio_hdr_t) + NL_MAX_PAYLOAD)

// Build a radio packet; returns its length, or 0 if it doesn't fit.
size_t nl_radio_build(uint8_t *out, size_t cap, uint8_t type, uint16_t seq, uint8_t role, uint8_t side,
                      const void *payload, size_t len);

// Check a received radio packet (magic, network, version). On success, *hdr
// is filled and *payload / *len point into in.
bool nl_radio_parse(const uint8_t *in, size_t n, nl_radio_hdr_t *hdr, const uint8_t **payload, size_t *len);

// ---------------------------------------------------------------- channel scanning
// Radio nodes don't know the channel in advance: WLED's channel follows its
// Wi-Fi (a phone hotspot at home, its own AP at a festival). They listen on
// each channel in turn until they hear an anchor heartbeat, stay there, and
// scan again if the anchor goes quiet.
#define NL_CHANNEL_MIN      1
#define NL_CHANNEL_MAX      13
#define NL_SCAN_DWELL_MS    400     // anchor heartbeats are 4 Hz, so at least one per dwell
#define NL_LOCK_TIMEOUT_MS  3000
// Fallback anchoring (the base, when no WLED usermod is running): after
// fallback_ms without hearing an anchor, anchor on the home channel. While
// anchoring, visit another channel for NL_PEEK_MS every NL_PEEK_EVERY_MS, in
// turn, to find a real anchor; on hearing one, stop anchoring and follow it.
#define NL_PEEK_EVERY_MS    3000
#define NL_PEEK_MS          300

typedef struct {
    uint8_t channel;
    bool locked;
    uint32_t on_channel_ms, since_heard_ms;
    uint32_t locks;         // times a channel was found (for stats)
    // fallback anchoring
    uint32_t fallback_ms;   // 0 = never anchor
    uint8_t home_channel, peek_channel;
    bool anchoring, peeking;
    uint32_t anchorings;    // times fallback anchoring started (for stats)
} nl_chanscan_t;

void nl_chanscan_init(nl_chanscan_t *s, uint8_t start_channel);
// Enable fallback anchoring on start_channel after fallback_ms of silence.
void nl_chanscan_set_fallback(nl_chanscan_t *s, uint32_t fallback_ms);
// Advance by dt_ms; returns the channel the radio should be on now.
uint8_t nl_chanscan_tick(nl_chanscan_t *s, uint32_t dt_ms);
// An anchor heartbeat was heard. Channels overlap, so it may have been heard
// from a neighbouring channel: move to the channel the anchor advertises
// (0 = unknown, stay put). Ends fallback anchoring.
void nl_chanscan_heard_anchor(nl_chanscan_t *s, uint8_t anchor_channel);
// The node is about to send something that matters (a command or bump): when
// fallback anchoring, return home now and put off the next peek for a full
// NL_PEEK_EVERY_MS. Returns the channel to be on.
uint8_t nl_chanscan_busy(nl_chanscan_t *s);

// ---------------------------------------------------------------- WLED audio
// The WLED usermod turns AudioReactive's analysis (smoothed volume 0..255, 16
// FFT bands 0..255, a beat-peak flag) into the same nl_audio_t the port eye
// sends, timing the peaks to estimate a tempo. Call about every 30 ms.
#define NL_AR_INTERVALS 8

typedef struct {
    uint32_t last_ms, last_peak_ms, beat_count;
    float avg_db;
    float intervals[NL_AR_INTERVALS];
    int pos, filled;
    bool peak_was;
} nl_ar_state_t;

void nl_ar_init(nl_ar_state_t *s);
void nl_ar_update(nl_ar_state_t *s, float volume, const uint8_t fft[16], bool peak, uint32_t now_ms, nl_audio_t *out);

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
