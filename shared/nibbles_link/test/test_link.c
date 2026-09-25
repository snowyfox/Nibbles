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

int main(void)
{
    srand(1);
    test_crc();
    test_round_trip();
    test_corruption();
    test_resync_and_stream();
    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
