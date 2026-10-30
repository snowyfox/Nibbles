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

#define NL_VERSION          4
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
    NL_MSG_BATTERY = 9,     // pack voltage (and current) at the pole base, base -> radio, ~5 Hz
    NL_MSG_FIN_POSE = 10,   // the fins' hinge angles, fin sensor node -> radio, ~50 Hz
    NL_MSG_FIN_STATE = 11,  // whether the lights follow the fins, and the angles in use, WLED -> radio, 2 Hz
    NL_MSG_WLED_STREAM = 12, // how steadily a streamed picture arrives, WLED -> radio, 1 Hz
    NL_MSG_EYE_LOOK = 13,   // an eye look from a WLED preset, WLED -> leader eye (radio) -> port eye (cable)
    NL_MSG_WLED_LAYERS = 14, // WLED's layer slots (nl_wled_layers_t), WLED -> radio, 2 Hz
} nl_msg_type_t;

typedef enum {
    NL_ROLE_NONE = 0,
    NL_ROLE_EYE_LEADER = 1, // runs the shared eye brain; the eyes' radio gateway
    NL_ROLE_EYE_EARS = 2,   // runs the mic and sends audio features
    NL_ROLE_BASE = 3,
    NL_ROLE_WLED = 4,
    NL_ROLE_FINS = 5,       // reads the fins' hinge angle sensors
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
    uint32_t peak_count;    // increments on every sound peak (raw onsets, many per beat);
                            // presets that react to every peak use this instead of beats
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
    uint8_t flags;          // NL_EYE_* below
} nl_eye_telemetry_t;

#define NL_EYE_AUTO_CYCLE   0x01    // presets change by themselves
#define NL_EYE_PEAKS_NOW    0x08    // the preset on screen reacts to every sound peak
#define NL_EYE_REACT(flags) (((flags) >> 1) & 3)  // nl_react_t in use
#define NL_EYE_REACT_FLAGS(r) (((r) & 3) << 1)

typedef enum {
    NL_REACT_PRESET = 0,    // each preset's own choice (preset_t.peak_beats)
    NL_REACT_BEATS = 1,     // every preset reacts to beats
    NL_REACT_PEAKS = 2,     // every preset reacts to every sound peak
} nl_react_t;

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
    NL_TARGET_FINS = 4,     // bumps only: WLED's fin segment (fin presets, see NL_FIN_PRESET_BASE)
} nl_target_t;

typedef enum {
    NL_OP_PRESET_SET = 1,   // arg = preset index
    NL_OP_PRESET_NEXT = 2,
    NL_OP_PRESET_PREV = 3,
    NL_OP_BRIGHTNESS_SET = 4,   // arg = 0..255
    NL_OP_BRIGHTNESS_STEP = 5,  // arg = signed step (eyes: through their brightness presets by sign)
    NL_OP_AUTO_CYCLE = 6,       // eyes: arg 1 = cycle presets by themselves, 0 = hold the current one
    NL_OP_REACTIVITY = 7,       // eyes: arg = nl_react_t
    NL_OP_FIN_FOLLOW = 8,       // WLED: fin LEDs follow the fins' angle: arg 0 off (locked at the modelled angle), 1 on, 2 toggle
    NL_OP_SLOT_LOOK = 9,        // WLED: arg = nl_slot_arg(slot, preset): the slot takes that preset's look and comes on; preset 0 switches it off
    NL_OP_SLOT_PALETTE = 10,    // WLED: arg = nl_slot_arg(slot, palette): the slot's palette (the effect keeps running)
} nl_op_t;

// NL_OP_PRESET_SET to WLED: arg | NL_PRESET_NO_EYES applies the preset to the
// body (and fins) only, skipping any eye settings stored in it (the base's
// BODY target). Without it a preset's "eyes" settings go to the eyes too.
#define NL_PRESET_NO_EYES   0x0100

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
    NL_BUMP_PRESET = 3,     // arg = preset number to show while held, 1-based on every
                            // receiver (WLED preset id; eye preset index + 1)
    NL_BUMP_PALETTE = 4,    // WLED: arg = nl_slot_arg(slot, palette): that palette while held
    NL_BUMP_LAYER = 5,      // WLED: arg = nl_slot_arg(slot, preset): the slot shows that preset's look while held
} nl_bump_action_t;

typedef enum {
    NL_BUMP_START = 1,
    NL_BUMP_HOLD = 2,
    NL_BUMP_STOP = 3,
} nl_bump_phase_t;

typedef struct NL_PACKED {
    uint16_t id;            // one id per press; all messages of a press share it
    uint8_t target;         // nl_target_t (bit mask: NL_TARGET_EYES | NL_TARGET_WLED | NL_TARGET_FINS)
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

// ---------------------------------------------------------------- WLED layer slots
// WLED 16 stacks overlapping segments; the Nibbles usermod keeps fixed ones as
// layers, slot n being segment n (wled/README.md, "Layer slots"). Bumps on the
// fin slots use NL_TARGET_FINS, the others NL_TARGET_WLED, so a fin bump and a
// body bump can be held together.
typedef enum {
    NL_SLOT_BODY = 0,       // the body's look (body presets)
    NL_SLOT_FINS = 1,       // the fin panel (fin presets)
    NL_SLOT_BODY_OVER = 2,  // over the body (screen blend)
    NL_SLOT_FINS_OVER = 3,  // over the fin panel
    NL_SLOT_BUMP = 4,       // the whole grid: flash and blackout bumps
    NL_SLOT_COUNT = 5,
} nl_slot_t;

// Layer looks for the overlay slots: WLED presets NL_LAYER_PRESET_BASE + n
// (n 1..NL_LAYER_PRESET_COUNT), usually sparse, cheap effects.
#define NL_LAYER_PRESET_BASE  100
#define NL_LAYER_PRESET_COUNT 16

// A slot and a value (preset 0..250 or palette 0..255) in one command or bump arg.
static inline int16_t nl_slot_arg(uint8_t slot, uint8_t value) { return (int16_t)(((slot & 0x7F) << 8) | value); }
static inline uint8_t nl_slot_arg_slot(int16_t arg) { return (uint8_t)((arg >> 8) & 0x7F); }
static inline uint8_t nl_slot_arg_value(int16_t arg) { return (uint8_t)(arg & 0xFF); }

typedef struct NL_PACKED {
    uint8_t on;             // shown (an existing segment, switched on)
    uint8_t look;           // the preset whose look it took last (0: none, or set directly)
    uint8_t fx;
    uint8_t palette;
    uint8_t blend;          // WLED blend mode (0 top, 2 add, 10 screen, 16 stencil, ...)
    uint8_t opacity;
} nl_slot_state_t;

typedef struct NL_PACKED {
    uint8_t count;          // slots in use (NL_SLOT_COUNT; 0: no 2D map, no slots)
    nl_slot_state_t slot[NL_SLOT_COUNT];
} nl_wled_layers_t;

// ---------------------------------------------------------------- eye looks
// An eye look stored in a WLED preset ("nibbles":{"eyes":{"look":{..}}}):
// everything the eyes' presets set (firmware/eyes/main/presets.h, preset_t),
// so the WLED preset is the one place a look is defined. WLED sends it when
// the preset is applied (a few times, same id); the leader eye shows it in its
// look slot (preset index = its built-in preset count) and passes it to the
// port eye over the cable. JSON keys are the field names below; colours are
// [r, g, b] 0..255. Spirals use the eyes' existing spiral shapes (arms and
// twist snap to the nearest built-in one).
#define NL_EYE_LOOK_PEAK_BEATS  0x01    // ripple on every sound peak, not every beat
#define NL_EYE_LOOK_RAINBOW     0x02    // rainbow colouring, not the palette
#define NL_EYE_LOOK_BANDS       0x04    // hard-edged bands, not glowing rings

typedef struct NL_PACKED {
    uint8_t id;             // the WLED preset it came from (repeats share it)
    uint8_t flags;          // NL_EYE_LOOK_*
    char name[NL_NAME_LEN];
    uint8_t palette[4][3];  // 0..255
    uint8_t rings, spiral_arms;
    float hue_spread, sat, palette_step, palette_music;
    float ring_width, halo_sigma, halo_amp, fill, flow, wobble_px;
    float pupil_scale, rim_amp, rim_white, ripple_pos, ripple_white;
    float spiral_twist, spin, spin_per_beat;
} nl_eye_look_t;

// ---------------------------------------------------------------- battery and power limit
// The lights' 5 V converter sits at the top of the pole and stutters when its
// input falls too low (measured: ~10.8 V at its terminals, see docs/power.md).
// During a full-white flash the pack sags and the pole wiring drops voltage,
// so WLED's current limit follows the pack voltage: the most power that keeps
// the converter's input above a floor.

#define NL_BATT_CURRENT     0x01    // current_ma is measured

typedef struct NL_PACKED {
    uint16_t pack_mv;       // battery voltage at the pole base
    int32_t current_ma;     // drawn by the lights (with NL_BATT_CURRENT)
    uint8_t flags;
} nl_battery_t;

typedef struct {
    uint16_t floor_mv;      // lowest input the converter may see (margin over where it stutters)
    uint16_t pack_mohm;     // pack, BMS and fuse: between the cells and where pack_mv is measured
    uint16_t wire_mohm;     // pole wiring there and back, from the base to the converter
    uint8_t efficiency_pct; // the converter's
    uint32_t min_ma;        // WLED limit range (5 V side)
    uint32_t max_ma;
} nl_power_model_t;

// 11.2 V floor (0.4 V over the measured 10.8 V), ~0.1 + 0.2 ohm (estimates),
// 90%, 3-17 A.
#define NL_POWER_MODEL_DEFAULT { 11200, 100, 200, 90, 3000, 17000 }

// WLED's limit (mA at 5 V) for a pack reading: the power P that keeps the
// converter at the floor, P = floor * (rest - floor) / (pack + wire), where
// rest is the pack's resting voltage (pack_mv plus the sag across the pack
// when the current is known). Clamped to [min_ma, max_ma].
uint32_t nl_power_limit_ma(const nl_power_model_t *m, uint32_t pack_mv, int32_t current_ma, bool have_current);

#define NL_POWER_RISE_MA_PER_S  2000    // a higher limit creeps up; a lower one applies at once

typedef struct {
    nl_power_model_t model;
    bool heard;             // any reading yet; until then the limiter stays out of the way
    uint32_t target_ma;     // from the latest reading
    uint32_t limit_ma;      // in force (follows target: down at once, up slowly)
    uint32_t last_ms;       // time of the latest reading
    uint32_t tick_ms;
    uint16_t pack_mv;
} nl_power_limiter_t;

void nl_power_limiter_init(nl_power_limiter_t *l, const nl_power_model_t *m);
// A battery reading at now_ms (pack_mv 0 forgets everything: back to "never heard").
void nl_power_limiter_reading(nl_power_limiter_t *l, const nl_battery_t *b, uint32_t now_ms);
// The limit to apply now, or 0 while no reading has been heard. Without new
// readings the last limit holds (the pack only empties), it never jumps back up.
uint32_t nl_power_limiter_tick(nl_power_limiter_t *l, uint32_t now_ms);

// ---------------------------------------------------------------- fin hinge angles
// Each fin swings ~180 degrees on its hinge, from straight up to straight
// down. Angles are calibrated to those limits: +9000 (centidegrees) at the up
// limit, -9000 at the down limit, so 0 is midway (not necessarily rest).

#define NL_FIN_PORT         0
#define NL_FIN_STARBOARD    1
#define NL_FIN_NONE         INT16_MIN   // no sensor, or not calibrated yet
#define NL_FIN_RAW_NONE     0xFFFF

typedef struct NL_PACKED {
    int16_t angle_cdeg[2];  // [NL_FIN_PORT], [NL_FIN_STARBOARD]: calibrated hinge angle, or NL_FIN_NONE
    uint16_t raw[2];        // the sensor's own reading (e.g. ADC counts), or NL_FIN_RAW_NONE
} nl_fin_pose_t;

typedef enum {
    NL_FIN_SRC_NONE = 0,    // no angle (no sensor heard)
    NL_FIN_SRC_SENSOR = 1,  // the fin sensor node's reading
    NL_FIN_SRC_SWEEP = 2,   // a test sweep (from the simulator)
    NL_FIN_SRC_LOCKED = 3,  // following is off: the modelled angle
} nl_fin_src_t;

// Fin presets: the fins' own segment (a panel beside the side view in WLED's
// 2D map) runs its own effect. WLED preset NL_FIN_PRESET_BASE + n (n 1..16)
// is fin preset n, saved with only the fin segment; NL_FIN_PRESET_BASE itself
// switches the fin segment off, so the fins show the body's colours again.
// Preset commands and bumps for the fins use those ids (bumps with
// NL_TARGET_FINS, so a fin bump and a body bump can be held together).
#define NL_FIN_PRESET_BASE  80
#define NL_FIN_PRESET_COUNT 16

typedef enum {
    NL_FIN_PROJ_SIDE = 0,   // fin panel seen from starboard: both fins on the same cells at equal angles
    NL_FIN_PROJ_TOP = 1,    // seen from above: port and starboard on opposite halves
} nl_fin_proj_t;

typedef struct NL_PACKED {
    uint8_t follow;         // 1: fin LEDs follow the fins' angle (NL_OP_FIN_FOLLOW)
    int16_t angle_cdeg[2];  // angle in use per fin ([NL_FIN_PORT], [NL_FIN_STARBOARD]), or NL_FIN_NONE
    uint8_t source[2];      // nl_fin_src_t per fin
    uint8_t fin_preset;     // the fin preset showing (1..NL_FIN_PRESET_COUNT), 0 = the fins follow the body
    uint8_t fin_proj;       // nl_fin_proj_t of the fin panel in use
} nl_fin_state_t;

// ---------------------------------------------------------------- streamed lights
// A WLED can show another WLED's picture, streamed to it (DDP, E1.31 or
// Art-Net: WLED's "realtime" mode) instead of running its own effects: the
// shark's controller showing frames rendered by a faster main board. A
// stream over Wi-Fi arrives in bursts with the odd stall; the display WLED
// times the gaps between incoming packets and reports them (NL_MSG_WLED_STREAM).
// A gap of NL_STREAM_RESTART_MS or more is the stream stopping and starting
// again, not a stall, and isn't counted.

#define NL_STREAM_SLOW_MS       50      // a late frame (2.5 frames at 50 fps)
#define NL_STREAM_STALL_MS      100     // a stall you can see
#define NL_STREAM_RESTART_MS    2000
#define NL_STREAM_WINDOW_S      60      // counts cover the last minute

typedef struct NL_PACKED {
    uint8_t active;         // 1 while showing a stream
    uint8_t source_ip[4];   // where the stream comes from (0s: none)
    uint16_t slow;          // gaps of >= NL_STREAM_SLOW_MS in the last minute
    uint16_t stalls;        // gaps of >= NL_STREAM_STALL_MS in the last minute
    uint16_t max_gap_ms;    // longest gap in the last minute
    uint32_t stalls_total;  // stalls since boot
} nl_wled_stream_t;

typedef struct {
    uint32_t second;        // which second (ms / 1000) this slot holds
    uint16_t slow, stalls, max_ms;
} nl_stream_second_t;

typedef struct {
    uint32_t last_ms;       // last packet (0: none yet)
    uint32_t stalls_total;
    nl_stream_second_t sec[NL_STREAM_WINDOW_S];  // one slot per second of the last minute
} nl_stream_gaps_t;

void nl_stream_gaps_init(nl_stream_gaps_t *g);
// A stream packet arrived at now_ms.
void nl_stream_gaps_packet(nl_stream_gaps_t *g, uint32_t now_ms);
// Fill the counts in *out (slow, stalls, max_gap_ms, stalls_total) for the
// minute up to now_ms; active and source_ip are left to the caller.
void nl_stream_gaps_summary(const nl_stream_gaps_t *g, uint32_t now_ms, nl_wled_stream_t *out);

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
    uint32_t last_ms, last_peak_ms, beat_count, peak_count;
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
