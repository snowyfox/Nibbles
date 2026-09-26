// Host check of the AudioReactive port: a synthetic 120 bpm kick + hi-hat
// track, printed as the values a WLED would receive.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/ar_sim.h"

int main(void)
{
    ar_config_t c = { .squelch = 10, .gain = 30, .agc = 2, .input_level = 128 };  // the shark's settings
    ar_init(&c);
    float block[AR_BLOCK];
    uint8_t pkt[AR_PACKET_SIZE];
    ar_output_t o;
    long n = 0;
    int peaks = 0;
    srand(1);
    for (int b = 0; b < 8 * AR_SAMPLE_RATE / AR_BLOCK; b++) {
        for (int i = 0; i < AR_BLOCK; i++, n++) {
            const double t = (double)n / AR_SAMPLE_RATE, tb = fmod(t, 0.5), th = fmod(t, 0.25);
            const double kick = 9000 * exp(-tb * 18) * sin(2 * M_PI * (55 + 90 * exp(-tb * 30)) * tb);
            const double hat = 1500 * exp(-th * 60) * ((rand() / (double)RAND_MAX) * 2 - 1);
            block[i] = (float)(t < 2.0 ? 0 : kick + hat);  // 2 s of silence first
        }
        ar_process_block(block, pkt, &o);
        peaks += o.peak;
        if (b % 43 == 0)
            printf("t %4.1fs vol %6.1f raw %6.1f agc %5.2f peak %d major %6.0f Hz | fft %3d %3d %3d %3d .. %3d %3d\n",
                   n / (double)AR_SAMPLE_RATE, o.volume_smooth, o.volume_raw, o.agc_gain, o.peak, o.fft_major_peak,
                   o.fft[0], o.fft[1], o.fft[2], o.fft[3], o.fft[14], o.fft[15]);
    }
    printf("peaks sent: %d in 6 s of music (12 kicks)\n", peaks);
    return 0;
}
