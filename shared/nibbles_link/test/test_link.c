// Host tests for the Nibbles link protocol. Run with `make` in this folder.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nibbles_link.h"

static int failures;

#define CHECK(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL: "); } else printf("ok:   "); \
    printf(__VA_ARGS__); printf("\n"); } while (0)

// Feed bytes; return the number of valid frames, copying the last one.
static int feed(nl_parser_t *p, const uint8_t *bytes, size_t n, nl_frame_t *last, uint8_t *payload_copy)
{
    int frames = 0;
    nl_frame_t f;
    for (size_t i = 0; i < n; i++) {
        if (nl_parse_byte(p, bytes[i], &f)) {
            frames++;
            if (last) *last = f;
            if (payload_copy) memcpy(payload_copy, f.payload, f.len);
        }
    }
    return frames;
}

static void test_round_trip(void)
{
    // Every payload length, with many zeros and 0xFF runs (COBS edge cases).
    int ok = 0, total = 0;
    for (size_t len = 0; len <= NL_MAX_PAYLOAD; len++) {
        uint8_t payload[NL_MAX_PAYLOAD], got[NL_MAX_PAYLOAD], frame[NL_MAX_FRAME];
        for (size_t i = 0; i < len; i++) payload[i] = (rand() % 3 == 0) ? 0 : (rand() % 2 ? 0xFF : (uint8_t)rand());
        const size_t n = nl_encode(NL_MSG_AUDIO, (uint16_t)(1000 + len), payload, len, frame, sizeof(frame));
        int zeros = 0;
        for (size_t i = 0; i + 1 < n; i++) zeros += frame[i] == 0;
        nl_parser_t p;
        nl_parser_init(&p);
        nl_frame_t f;
        const int frames = feed(&p, frame, n, &f, got);
        total++;
        if (n > 0 && zeros == 0 && frame[n - 1] == 0 && frames == 1 && f.type == NL_MSG_AUDIO &&
            f.version == NL_VERSION && f.seq == 1000 + len && f.len == len && memcmp(got, payload, len) == 0) ok++;
    }
    CHECK(ok == total, "round trip, payloads of 0..%d bytes: %d/%d intact", NL_MAX_PAYLOAD, ok, total);

    uint8_t big[NL_MAX_PAYLOAD + 1] = { 0 }, frame[NL_MAX_FRAME];
    CHECK(nl_encode(1, 0, big, sizeof(big), frame, sizeof(frame)) == 0, "oversized payload refused");
}

static void test_corruption(void)
{
    nl_audio_t a = { .level_db = -70.0f, .loudness = 0.5f, .beat_count = 42 };
    uint8_t frame[NL_MAX_FRAME];
    const size_t n = nl_encode(NL_MSG_AUDIO, 7, &a, sizeof(a), frame, sizeof(frame));
    int caught = 0;
    for (size_t i = 0; i + 1 < n; i++) {  // flip each byte (not the delimiter)
        uint8_t bad[NL_MAX_FRAME];
        memcpy(bad, frame, n);
        bad[i] ^= 0x5A;
        if (bad[i] == 0) bad[i] = 0x01;  // keep it one frame
        nl_parser_t p;
        nl_parser_init(&p);
        if (feed(&p, bad, n, NULL, NULL) == 0 && p.crc_errors + p.bad_frames == 1) caught++;
    }
    CHECK(caught == (int)n - 1, "every single corrupted byte detected (%d/%d)", caught, (int)n - 1);
}

static void test_resync_and_stream(void)
{
    // Garbage, a frame split across feeds, a truncated frame, then good frames.
    uint8_t stream[2048];
    size_t n = 0;
    for (int i = 0; i < 300; i++) stream[n++] = (uint8_t)(rand() | 1);  // noise, no delimiter
    stream[n++] = 0;
    nl_eye_state_t e = { .time_s = 12.5f, .hue = 200.0f, .preset = 5, .side = NL_SIDE_STARBOARD };
    uint8_t frame[NL_MAX_FRAME];
    size_t fl = nl_encode(NL_MSG_EYE_STATE, 1, &e, sizeof(e), frame, sizeof(frame));
    memcpy(stream + n, frame, fl / 2);  // truncated: cut short by the next frame's bytes
    n += fl / 2;
    for (int k = 0; k < 5; k++) {
        e.preset = (uint8_t)k;
        fl = nl_encode(NL_MSG_EYE_STATE, (uint16_t)(2 + k), &e, sizeof(e), frame, sizeof(frame));
        memcpy(stream + n, frame, fl);
        n += fl;
    }
    nl_parser_t p;
    nl_parser_init(&p);
    nl_frame_t f;
    uint8_t got[NL_MAX_PAYLOAD];
    // Feed in uneven chunks.
    int frames = 0;
    for (size_t i = 0; i < n;) {
        const size_t chunk = 1 + (size_t)(rand() % 17);
        const size_t take = i + chunk > n ? n - i : chunk;
        frames += feed(&p, stream + i, take, &f, got);
        i += take;
    }
    nl_eye_state_t last;
    memcpy(&last, got, sizeof(last));
    // The truncated frame merges with the next one and fails its CRC, so 4 of the 5 survive.
    CHECK(frames == 4 && last.preset == 4 && f.seq == 6, "resync after noise and a truncated frame: %d frames, last preset %d",
          frames, last.preset);
    CHECK(p.crc_errors + p.bad_frames >= 1, "the garbage was counted as errors (%u crc, %u bad)",
          (unsigned)p.crc_errors, (unsigned)p.bad_frames);
}

static void test_crc(void)
{
    // CRC-16/CCITT-FALSE check value.
    CHECK(nl_crc16((const uint8_t *)"123456789", 9) == 0x29B1, "CRC-16/CCITT-FALSE check value 0x%04X", nl_crc16((const uint8_t *)"123456789", 9));
}

static void test_radio(void)
{
    uint8_t pkt[NL_RADIO_MAX];
    const nl_cmd_t cmd = { .id = 77, .target = NL_TARGET_EYES, .op = NL_OP_PRESET_NEXT };
    const size_t n = nl_radio_build(pkt, sizeof(pkt), NL_MSG_CMD, 9, NL_ROLE_BASE, NL_SIDE_NONE, &cmd, sizeof(cmd));
    nl_radio_hdr_t h;
    const uint8_t *pl;
    size_t len;
    const bool ok = nl_radio_parse(pkt, n, &h, &pl, &len);
    nl_cmd_t got;
    memcpy(&got, pl, sizeof(got));
    CHECK(ok && h.type == NL_MSG_CMD && h.seq == 9 && h.role == NL_ROLE_BASE && len == sizeof(cmd) && got.id == 77 &&
          got.op == NL_OP_PRESET_NEXT, "radio packet round trip (%d bytes)", (int)n);
    pkt[2] ^= 1;  // another network
    CHECK(!nl_radio_parse(pkt, n, &h, &pl, &len), "packet from another network ignored");
    pkt[2] ^= 1;
    pkt[5] = NL_VERSION + 1;
    CHECK(!nl_radio_parse(pkt, n, &h, &pl, &len), "packet with another protocol version ignored");
    CHECK(!nl_radio_parse(pkt, 5, &h, &pl, &len), "short packet ignored");
}

static void test_chanscan(void)
{
    // An anchor on channel 11: found within one sweep, then held.
    nl_chanscan_t s;
    nl_chanscan_init(&s, 1);
    int found_ms = -1;
    for (int t = 0; t < 20000; t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        if (ch == 11 && (t % 250) < 50) nl_chanscan_heard_anchor(&s);  // 4 Hz heartbeat
        if (s.locked && found_ms < 0) found_ms = t;
    }
    CHECK(found_ms >= 0 && found_ms <= 13 * NL_SCAN_DWELL_MS, "anchor on channel 11 found after %d ms", found_ms);
    CHECK(s.locked && s.channel == 11 && s.locks == 1, "stays locked on channel 11 (%u locks)", (unsigned)s.locks);

    // The anchor moves to channel 3 (WLED switched from hotspot to its own AP).
    int relock_ms = -1;
    for (int t = 0; t < 30000; t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        if (ch == 3 && (t % 250) < 50) nl_chanscan_heard_anchor(&s);
        if (s.locked && s.channel == 3 && relock_ms < 0) relock_ms = t;
    }
    CHECK(relock_ms >= NL_LOCK_TIMEOUT_MS && relock_ms <= NL_LOCK_TIMEOUT_MS + 13 * NL_SCAN_DWELL_MS,
          "anchor moved to channel 3: relocked after %d ms", relock_ms);

    // No anchor: keeps sweeping every channel.
    nl_chanscan_init(&s, 5);
    int seen[NL_CHANNEL_MAX + 1] = { 0 };
    for (int t = 0; t < 13 * NL_SCAN_DWELL_MS; t += 50) seen[nl_chanscan_tick(&s, 50)] = 1;
    int all = 1;
    for (int c = NL_CHANNEL_MIN; c <= NL_CHANNEL_MAX; c++) all &= seen[c];
    CHECK(all && !s.locked, "no anchor: sweeps all 13 channels");
}

int main(void)
{
    srand(1);
    test_crc();
    test_round_trip();
    test_corruption();
    test_resync_and_stream();
    test_radio();
    test_chanscan();
    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
