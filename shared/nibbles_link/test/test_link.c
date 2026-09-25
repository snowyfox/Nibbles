// Host tests for the Nibbles link protocol. Run with `make` in this folder.
#include <math.h>
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
        if (ch == 11 && (t % 250) < 50) nl_chanscan_heard_anchor(&s, 11);  // 4 Hz heartbeat
        if (s.locked && found_ms < 0) found_ms = t;
    }
    CHECK(found_ms >= 0 && found_ms <= 13 * NL_SCAN_DWELL_MS, "anchor on channel 11 found after %d ms", found_ms);
    CHECK(s.locked && s.channel == 11 && s.locks == 1, "stays locked on channel 11 (%u locks)", (unsigned)s.locks);

    // The anchor moves to channel 3 (WLED switched from hotspot to its own AP).
    int relock_ms = -1;
    for (int t = 0; t < 30000; t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        if (ch == 3 && (t % 250) < 50) nl_chanscan_heard_anchor(&s, 3);
        if (s.locked && s.channel == 3 && relock_ms < 0) relock_ms = t;
    }
    CHECK(relock_ms >= NL_LOCK_TIMEOUT_MS && relock_ms <= NL_LOCK_TIMEOUT_MS + 13 * NL_SCAN_DWELL_MS,
          "anchor moved to channel 3: relocked after %d ms", relock_ms);

    // Heard from a neighbouring channel (channels overlap): jump to the
    // channel the anchor advertises instead of locking where it was heard.
    nl_chanscan_init(&s, 1);
    int landed = 0;
    for (int t = 0; t < 20000 && !landed; t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        if ((ch == 5 || ch == 6 || ch == 7) && (t % 250) < 50) nl_chanscan_heard_anchor(&s, 6);
        if (s.locked) landed = s.channel;
    }
    CHECK(landed == 6, "anchor on 6 first heard from channel 5: locked on %d", landed);

    // No anchor: keeps sweeping every channel.
    nl_chanscan_init(&s, 5);
    int seen[NL_CHANNEL_MAX + 1] = { 0 };
    for (int t = 0; t < 13 * NL_SCAN_DWELL_MS; t += 50) seen[nl_chanscan_tick(&s, 50)] = 1;
    int all = 1;
    for (int c = NL_CHANNEL_MIN; c <= NL_CHANNEL_MAX; c++) all &= seen[c];
    CHECK(all && !s.locked, "no anchor: sweeps all 13 channels");
}

static void test_fallback_anchor(void)
{
    // No anchor anywhere: anchors on the home channel after the fallback time.
    nl_chanscan_t s;
    nl_chanscan_init(&s, 6);
    nl_chanscan_set_fallback(&s, 12000);
    int start_ms = -1;
    for (int t = 0; t < 20000 && start_ms < 0; t += 50) {
        nl_chanscan_tick(&s, 50);
        if (s.anchoring) start_ms = t;
    }
    CHECK(start_ms >= 12000 - 100 && start_ms <= 12100 && s.channel == 6, "fallback anchoring after %d ms on %d",
          start_ms, s.channel);

    // While anchoring it is on the home channel most of the time and visits
    // every other channel.
    int home = 0, total = 0, seen[NL_CHANNEL_MAX + 1] = { 0 };
    for (int t = 0; t < 12 * (NL_PEEK_EVERY_MS + NL_PEEK_MS); t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        seen[ch] = 1;
        home += ch == 6;
        total++;
    }
    int all = 1;
    for (int c = NL_CHANNEL_MIN; c <= NL_CHANNEL_MAX; c++) all &= seen[c];
    CHECK(all && home * 100 / total >= 85, "anchoring: peeks at all channels, home %d%% of the time", home * 100 / total);

    // Sending bumps keeps it home: no peeks while HOLDs go out every 100 ms,
    // and a peek in progress ends at once.
    int away = 0;
    for (int t = 0; t < 10000; t += 50) {
        uint8_t ch = nl_chanscan_tick(&s, 50);
        if (t % 100 == 0) ch = nl_chanscan_busy(&s);
        away += ch != 6;
    }
    CHECK(away == 0, "anchoring: no peeks while sending (%d ticks away)", away);

    // WLED appears on channel 11: found on a peek, anchoring ends, follows it.
    int found_ms = -1;
    for (int t = 0; t < 60000 && found_ms < 0; t += 50) {
        const uint8_t ch = nl_chanscan_tick(&s, 50);
        if (ch == 11 && (t % 250) < 50) nl_chanscan_heard_anchor(&s, 11);
        if (!s.anchoring) found_ms = t;
    }
    CHECK(found_ms >= 0 && found_ms <= 12 * (NL_PEEK_EVERY_MS + NL_PEEK_MS) && s.locked && s.channel == 11,
          "real anchor found while anchoring after %d ms, now on %d", found_ms, s.channel);

    // Following it: no anchoring while it keeps talking.
    for (int t = 0; t < 30000; t += 50) {
        nl_chanscan_tick(&s, 50);
        if ((t % 250) < 50) nl_chanscan_heard_anchor(&s, 11);
    }
    CHECK(!s.anchoring && s.channel == 11, "stays with the real anchor");

    // Without the fallback set, never anchors.
    nl_chanscan_init(&s, 6);
    for (int t = 0; t < 60000; t += 50) nl_chanscan_tick(&s, 50);
    CHECK(!s.anchoring, "no fallback: never anchors");
}

static void test_wled_audio(void)
{
    nl_ar_state_t s;
    nl_ar_init(&s);
    nl_audio_t a;
    uint8_t bassy[16] = { 200, 200, 180, 160, 80, 60, 40, 30, 20, 20, 15, 10, 10, 5, 5, 5 };
    // 128 bpm kick: a peak (one 32 ms frame) every 469 ms.
    uint32_t beats = 0;
    for (uint32_t t = 0; t < 8000; t += 32) {
        const bool peak = (t % 469) < 32;
        nl_ar_update(&s, 180.0f, bassy, peak, t, &a);
        beats += peak;
    }
    CHECK(fabsf(60.0f / a.beat_period_s - 128.0f) < 6.0f && a.beat_confidence > 0.8f,
          "WLED audio: 128 bpm peaks -> %.0f bpm, confidence %.2f", a.beat_period_s > 0 ? 60.0f / a.beat_period_s : 0.0f,
          a.beat_confidence);
    CHECK(a.beat_count >= beats - 1 && a.warmth > 0.8f && a.loudness > 0.6f,
          "WLED audio: %lu beats, warmth %.2f, loudness %.2f", (unsigned long)a.beat_count, a.warmth, a.loudness);

    // 70 bpm half-time folds into the eyes' 100..200 range (140).
    nl_ar_init(&s);
    for (uint32_t t = 0; t < 12000; t += 32) nl_ar_update(&s, 150.0f, bassy, (t % 857) < 32, t, &a);
    CHECK(fabsf(60.0f / a.beat_period_s - 140.0f) < 8.0f, "WLED audio: 70 bpm folds to %.0f bpm", 60.0f / a.beat_period_s);

    // Random peaks: no tempo.
    nl_ar_init(&s);
    srand(7);
    for (uint32_t t = 0; t < 10000; t += 32) nl_ar_update(&s, 120.0f, bassy, rand() % 12 == 0, t, &a);
    CHECK(a.beat_period_s == 0.0f, "WLED audio: random peaks give no tempo (confidence %.2f)", a.beat_confidence);

    // Silence after music: the tempo is dropped.
    nl_ar_init(&s);
    for (uint32_t t = 0; t < 6000; t += 32) nl_ar_update(&s, 180.0f, bassy, (t % 469) < 32, t, &a);
    for (uint32_t t = 6000; t < 10000; t += 32) nl_ar_update(&s, 0.0f, bassy, false, t, &a);
    CHECK(a.beat_period_s == 0.0f && a.loudness == 0.0f && a.level_db < -40.0f, "WLED audio: silence drops the tempo (%.0f dB)",
          a.level_db);
}

static void test_bump(void)
{
    nl_bump_rx_t rx;
    nl_bump_rx_init(&rx);
    nl_bump_t b = { .id = 1, .target = NL_TARGET_WLED, .action = NL_BUMP_FLASH, .phase = NL_BUMP_START, .intensity = 255 };
    // Normal press: start, keepalives, stop.
    int begins = 0, ends = 0;
    uint32_t t = 0;
    begins += nl_bump_rx_message(&rx, &b, t) == NL_BUMP_EV_BEGIN;
    b.phase = NL_BUMP_HOLD;
    for (t = 100; t <= 1000; t += 100) {
        begins += nl_bump_rx_message(&rx, &b, t) == NL_BUMP_EV_BEGIN;
        ends += nl_bump_rx_tick(&rx, t + 50) == NL_BUMP_EV_END;
    }
    b.phase = NL_BUMP_STOP;
    ends += nl_bump_rx_message(&rx, &b, t) == NL_BUMP_EV_END;
    CHECK(begins == 1 && ends == 1 && !rx.active, "held bump: one begin, held through keepalives, one end");

    // START lost: the first keepalive starts it.
    b.id = 2; b.phase = NL_BUMP_HOLD;
    CHECK(nl_bump_rx_message(&rx, &b, 2000) == NL_BUMP_EV_BEGIN, "lost START: first keepalive begins the bump");
    // STOP lost: released by timeout, not before.
    const bool held = nl_bump_rx_tick(&rx, 2000 + NL_BUMP_TIMEOUT_MS - 1) == NL_BUMP_EV_NONE;
    const bool released = nl_bump_rx_tick(&rx, 2000 + NL_BUMP_TIMEOUT_MS) == NL_BUMP_EV_END;
    CHECK(held && released && !rx.active, "lost STOP: released after %d ms without messages", NL_BUMP_TIMEOUT_MS);
    // A late STOP for the finished press is ignored.
    b.phase = NL_BUMP_STOP;
    CHECK(nl_bump_rx_message(&rx, &b, 2500) == NL_BUMP_EV_NONE, "late STOP ignored");

    // A new press while one is active replaces it.
    b.id = 3; b.phase = NL_BUMP_START;
    nl_bump_rx_message(&rx, &b, 3000);
    b.id = 4; b.action = NL_BUMP_BLACKOUT;
    CHECK(nl_bump_rx_message(&rx, &b, 3050) == NL_BUMP_EV_REPLACE && rx.bump.action == NL_BUMP_BLACKOUT,
          "new press replaces the active bump");
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
    test_fallback_anchor();
    test_bump();
    test_wled_audio();
    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
