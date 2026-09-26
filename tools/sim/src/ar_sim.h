// WLED AudioReactive's sender-side sound processing, ported to plain C so a
// computer can play the part of a WLED with a microphone: it turns audio into
// the 44-byte "audio sync" packets a WLED in receive mode feeds to its effects.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AR_SAMPLE_RATE  22050
#define AR_BLOCK        512      // samples per FFT block (23.2 ms)
#define AR_PACKET_SIZE  44

typedef struct {
    int squelch;       // WLED "squelch" (default 10)
    int gain;          // WLED "gain" (sampleGain, default 60)
    int agc;           // 0 off, 1 normal, 2 vivid, 3 lazy
    int input_level;   // UI "input level" slider, 0..255 (default 128)
} ar_config_t;

// What WLED's effects see, for display.
typedef struct {
    float volume_smooth, volume_raw, fft_magnitude, fft_major_peak, agc_gain;
    bool peak;
    uint8_t fft[16];
} ar_output_t;

void ar_init(const ar_config_t *cfg);

// One block of AR_BLOCK samples at AR_SAMPLE_RATE, scaled like WLED's I2S
// samples (16-bit range: full scale = 32767). Runs the FFT and the 2 ms
// sampling/AGC filters for the block's duration, and fills packet (the audio
// sync packet, format v2) and optionally out.
void ar_process_block(const float *samples, uint8_t packet[AR_PACKET_SIZE], ar_output_t *out);
