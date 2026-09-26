// Derived from WLED's AudioReactive usermod (usermods/audioreactive/
// audio_reactive.cpp, WLED 16.0.1, EUPL-1.2): FFTcode(), fftAddAvg(),
// postProcessFFTResults(), detectSamplePeak(), autoResetPeak(), getSample(),
// agcAvg() and transmitAudioData(), with ArduinoFFT's Blackman-Harris window
// and majorPeak(). Behaviour kept as close as practical; ESP32 tasks and I2S
// replaced by a block-at-a-time call with simulated time.
#include "ar_sim.h"

#include <math.h>
#include <string.h>

#define N      AR_BLOCK
#define HALF   (AR_BLOCK / 2)
#define NCH    16
#define PI_D   3.14159265358979323846

// ESP32 build defaults (FFT_PREFER_EXACT_PEAKS, Blackman-Harris, FFTScalingMode 3).
#define FFT_DOWNSCALE 0.40f
static const float fft_pink[NCH] = { 1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f,
                                     1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f };
static const int scaling_mode = 3;
static const bool limiter_on = true;
static const int decay_time = 1400;
static const uint8_t max_vol = 31, bin_num = 8;

// AGC presets: normal, vivid, lazy.
static const double agc_sample_decay[3] = { 0.9994, 0.9985, 0.9997 };
static const float agc_zone_low[3] = { 32, 28, 36 }, agc_zone_high[3] = { 240, 240, 248 };
static const float agc_zone_stop[3] = { 336, 448, 304 };
static const float agc_target0[3] = { 112, 144, 164 }, agc_target0_up[3] = { 88, 64, 116 };
static const float agc_target1[3] = { 220, 224, 216 };
static const double agc_follow_fast[3] = { 1 / 192.0, 1 / 128.0, 1 / 256.0 };
static const double agc_follow_slow[3] = { 1 / 6144.0, 1 / 4096.0, 1 / 8192.0 };
static const double agc_kp[3] = { 0.6, 1.5, 0.65 }, agc_ki[3] = { 1.7, 1.85, 1.2 };
static const float agc_sample_smooth[3] = { 1 / 12.0f, 1 / 6.0f, 1 / 16.0f };

static ar_config_t cfg;
static float window[N];
static float re[N], im[N];
static uint32_t now_ms;  // simulated clock

// AudioReactive state (same names as the original)
static float micDataReal, multAgc = 1.0f, sampleAvg, sampleAgc, rawSampleAgc, sampleReal, sampleMax;
static float micLev, expAdjF, control_integrated;
static int16_t sampleRaw;
static float FFT_MajorPeak = 1.0f, FFT_Magnitude;
static bool samplePeak, udpSamplePeak;
static uint32_t timeOfPeak;
static float fftCalc[NCH], fftAvg[NCH];
static uint8_t fftResult[NCH];
static int last_soundAgc = -1;
static uint32_t agc_last_time;

static float constrainf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void ar_init(const ar_config_t *c)
{
    cfg = *c;
    for (int i = 0; i < N; i++) {  // ArduinoFFT Blackman-Harris
        const double r = (double)i / (N - 1);
        window[i] = (float)(0.35875 - 0.48829 * cos(2 * PI_D * r) + 0.14128 * cos(4 * PI_D * r) - 0.01168 * cos(6 * PI_D * r));
    }
    now_ms = 1000;
}

// In-place radix-2 complex FFT.
static void fft(float *xr, float *xi)
{
    for (int i = 1, j = 0; i < N; i++) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = xr[i]; xr[i] = xr[j]; xr[j] = t;
            t = xi[i]; xi[i] = xi[j]; xi[j] = t;
        }
    }
    for (int len = 2; len <= N; len <<= 1) {
        const double ang = -2 * PI_D / len;
        const float wr = (float)cos(ang), wi = (float)sin(ang);
        for (int i = 0; i < N; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                const int a = i + k, b = a + len / 2;
                const float tr = xr[b] * cr - xi[b] * ci, ti = xr[b] * ci + xi[b] * cr;
                xr[b] = xr[a] - tr; xi[b] = xi[a] - ti;
                xr[a] += tr; xi[a] += ti;
                const float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

static float fft_add_avg(int from, int to)
{
    float result = 0;
    for (int i = from; i <= to; i++) result += re[i];
    result *= 0.0625f;
    return result / (float)(to - from + 1);
}

static void auto_reset_peak(void)
{
    const uint32_t peak_delay = 50;  // max(50, frame time); frame time ~23 ms at 43 fps
    if (now_ms - timeOfPeak > peak_delay) samplePeak = false;
}

static void detect_sample_peak(void)
{
    if (sampleAvg > 1 && max_vol > 0 && bin_num > 4 && re[bin_num] > max_vol && now_ms - timeOfPeak > 100) {
        samplePeak = true;
        timeOfPeak = now_ms;
        udpSamplePeak = true;
    }
}

static void post_process(bool noise_gate_open)
{
    for (int i = 0; i < NCH; i++) {
        if (noise_gate_open) {
            fftCalc[i] *= fft_pink[i];
            if (scaling_mode > 0) fftCalc[i] *= FFT_DOWNSCALE;
            fftCalc[i] *= cfg.agc ? multAgc : ((float)cfg.gain / 40.0f * (float)cfg.input_level / 128.0f + 1.0f / 16.0f);
            if (fftCalc[i] < 0) fftCalc[i] = 0;
        }
        if (fftCalc[i] > fftAvg[i]) fftAvg[i] = fftCalc[i] * 0.75f + 0.25f * fftAvg[i];
        else if (decay_time < 1000) fftAvg[i] = fftCalc[i] * 0.22f + 0.78f * fftAvg[i];
        else if (decay_time < 2000) fftAvg[i] = fftCalc[i] * 0.17f + 0.83f * fftAvg[i];
        else if (decay_time < 3000) fftAvg[i] = fftCalc[i] * 0.14f + 0.86f * fftAvg[i];
        else fftAvg[i] = fftCalc[i] * 0.1f + 0.9f * fftAvg[i];
        fftCalc[i] = constrainf(fftCalc[i], 0.0f, 1023.0f);
        fftAvg[i] = constrainf(fftAvg[i], 0.0f, 1023.0f);

        float r = limiter_on ? fftAvg[i] : fftCalc[i];
        // square root scaling (mode 3)
        r *= 0.38f;
        r -= 6.0f;
        r = r > 1.0f ? sqrtf(r) : 0.0f;
        r *= 0.85f + (float)i / 4.5f;
        r = r * 255.0f / 16.0f;  // mapf(r, 0, 16, 0, 255)
        if (cfg.agc > 0) {
            float post_gain = (float)cfg.input_level / 128.0f;
            if (post_gain < 1.0f) post_gain = ((post_gain - 1.0f) * 0.8f) + 1.0f;
            r *= post_gain;
        }
        const int v = (int)r;
        fftResult[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}

// One run of FFTcode()'s loop body on the current block (in re[]).
static void fft_task(void)
{
    float maxSample = 0;
    for (int i = 0; i < N; i++) {
        const float v = re[i];
        if (v <= 32767 - 1024 && v >= -32768 + 1024 && fabsf(v) > maxSample) maxSample = fabsf(v);
    }
    micDataReal = maxSample;

    if (sampleAvg > 0.25f) {
        float mean = 0;  // ArduinoFFT dcRemoval()
        for (int i = 0; i < N; i++) mean += re[i];
        mean /= N;
        for (int i = 0; i < N; i++) {
            re[i] = (re[i] - mean) * window[i];
            im[i] = 0.0f;
        }
        fft(re, im);
        for (int i = 0; i <= HALF; i++) re[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
        re[0] = 0;
        // ArduinoFFT majorPeak(): parabolic interpolation around the biggest bin
        float maxY = 0;
        int idx = 1;
        for (int i = 1; i < HALF; i++)
            if (re[i - 1] < re[i] && re[i] >= re[i + 1] && re[i] > maxY) { maxY = re[i]; idx = i; }
        const float den = re[idx - 1] - 2.0f * re[idx] + re[idx + 1];
        const float delta = den != 0.0f ? 0.5f * (re[idx - 1] - re[idx + 1]) / den : 0.0f;
        FFT_MajorPeak = ((idx + delta) * AR_SAMPLE_RATE) / (N - 1);
        FFT_Magnitude = fabsf(den);
        FFT_MajorPeak = constrainf(FFT_MajorPeak, 1.0f, 11025.0f);
    } else {
        memset(re, 0, sizeof(re));
        FFT_MajorPeak = 1;
        FFT_Magnitude = 0.001f;
    }

    if (fabsf(sampleAvg) > 0.5f) {
        fftCalc[0] = fft_add_avg(1, 2);
        fftCalc[1] = fft_add_avg(2, 3);
        fftCalc[2] = fft_add_avg(3, 5);
        fftCalc[3] = fft_add_avg(5, 7);
        fftCalc[15] = fft_add_avg(165, 215) * 0.70f;
        fftCalc[4] = fft_add_avg(7, 10);
        fftCalc[5] = fft_add_avg(10, 13);
        fftCalc[6] = fft_add_avg(13, 19);
        fftCalc[7] = fft_add_avg(19, 26);
        fftCalc[8] = fft_add_avg(26, 33);
        fftCalc[9] = fft_add_avg(33, 44);
        fftCalc[10] = fft_add_avg(44, 56);
        fftCalc[11] = fft_add_avg(56, 70);
        fftCalc[12] = fft_add_avg(70, 86);
        fftCalc[13] = fft_add_avg(86, 104);
        fftCalc[14] = fft_add_avg(104, 165) * 0.88f;
    } else {
        for (int i = 0; i < NCH; i++) {
            fftCalc[i] *= 0.85f;
            if (fftCalc[i] < 4.0f) fftCalc[i] = 0.0f;
        }
    }
    post_process(fabsf(sampleAvg) > 0.25f);
    auto_reset_peak();
    detect_sample_peak();
}

static void get_sample(void)
{
    const float weighting = 0.2f;
    const int preset = cfg.agc > 0 ? cfg.agc - 1 : 0;
    micLev += (micDataReal - micLev) / 12288.0f;
    if ((int)micDataReal < micLev) micLev = ((micLev * 31.0f) + micDataReal) / 32.0f;
    const float micInNoDC = fabsf(micDataReal - micLev);
    expAdjF = weighting * micInNoDC + (1.0f - weighting) * expAdjF;
    expAdjF = fabsf(expAdjF);
    expAdjF = expAdjF <= cfg.squelch ? 0 : expAdjF;
    if (cfg.squelch == 0 && expAdjF < 0.25f) expAdjF = 0;
    const float tmp = expAdjF;
    float adj = tmp * cfg.gain / 40.0f * cfg.input_level / 128.0f + tmp / 16.0f;
    sampleReal = tmp;
    adj = fmaxf(fminf(adj, 255), 0);
    sampleRaw = (int16_t)adj;
    if (sampleMax < sampleReal && sampleReal > 0.5f) {
        sampleMax = sampleMax + 0.5f * (sampleReal - sampleMax);
        if ((bin_num < 12 || max_vol < 1) && now_ms - timeOfPeak > 80 && sampleAvg > 1) {
            samplePeak = true;
            timeOfPeak = now_ms;
            udpSamplePeak = true;
        }
    } else if (multAgc * sampleMax > agc_zone_stop[preset] && cfg.agc > 0) {
        sampleMax += 0.5f * (sampleReal - sampleMax);
    } else {
        sampleMax *= (float)agc_sample_decay[preset];
    }
    if (sampleMax < 0.5f) sampleMax = 0.0f;
    sampleAvg = ((sampleAvg * 15.0f) + adj) / 16.0f;
    sampleAvg = fabsf(sampleAvg);
}

static void agc_avg(void)
{
    const int preset = cfg.agc > 0 ? cfg.agc - 1 : 0;
    const float lastMultAgc = multAgc;
    float multAgcTemp = multAgc;
    float tmpAgc;
    if (last_soundAgc != cfg.agc) control_integrated = 0.0f;
    if (now_ms - agc_last_time > 2) {
        agc_last_time = now_ms;
        if (fabsf(sampleReal) < 2.0f || sampleMax < 1.0f) {
            if (fabsf(control_integrated) < 0.01f) control_integrated = 0.0f;
            else control_integrated *= 0.91f;
        } else {
            tmpAgc = sampleReal * multAgc;
            multAgcTemp = tmpAgc <= agc_target0_up[preset] ? agc_target0[preset] / sampleMax : agc_target1[preset] / sampleMax;
        }
        multAgcTemp = constrainf(multAgcTemp, 1.0f / 64.0f, 32.0f);
        const float control_error = multAgcTemp - lastMultAgc;
        if (multAgcTemp > 0.085f && multAgcTemp < 6.5f && multAgc * sampleMax < agc_zone_stop[preset])
            control_integrated += control_error * 0.002f * 0.25f;
        else
            control_integrated *= 0.9f;
        tmpAgc = sampleReal * lastMultAgc;
        if (tmpAgc > agc_zone_high[preset] || tmpAgc < cfg.squelch + agc_zone_low[preset]) {
            multAgcTemp = lastMultAgc + (float)(agc_follow_fast[preset] * agc_kp[preset]) * control_error;
            multAgcTemp += (float)(agc_follow_fast[preset] * agc_ki[preset]) * control_integrated;
        } else {
            multAgcTemp = lastMultAgc + (float)(agc_follow_slow[preset] * agc_kp[preset]) * control_error;
            multAgcTemp += (float)(agc_follow_slow[preset] * agc_ki[preset]) * control_integrated;
        }
        multAgcTemp = constrainf(multAgcTemp, 1.0f / 64.0f, 32.0f);
    }
    tmpAgc = sampleReal * multAgcTemp;
    if (fabsf(sampleReal) < 2.0f) tmpAgc = 0.0f;
    if (tmpAgc > 255) tmpAgc = 255.0f;
    if (tmpAgc < 1) tmpAgc = 0.0f;
    multAgc = multAgcTemp;
    rawSampleAgc = 0.8f * tmpAgc + 0.2f * rawSampleAgc;
    if (fabsf(tmpAgc) < 1.0f) sampleAgc = 0.5f * tmpAgc + 0.5f * sampleAgc;
    else sampleAgc += agc_sample_smooth[preset] * (tmpAgc - sampleAgc);
    sampleAgc = fabsf(sampleAgc);
    last_soundAgc = cfg.agc;
}

static void put_f32(uint8_t *p, float v) { memcpy(p, &v, 4); }  // little-endian, as on the ESP32

void ar_process_block(const float *samples, uint8_t packet[AR_PACKET_SIZE], ar_output_t *out)
{
    memcpy(re, samples, sizeof(re));
    fft_task();
    // The usermod loop runs its filters every ~2 ms; one block lasts 23.2 ms.
    for (int k = 0; k < 11; k++) {
        now_ms += 2;
        get_sample();
        agc_avg();
    }
    now_ms += 1;  // 23 ms per block in total
    auto_reset_peak();

    const float volumeSmth = cfg.agc ? sampleAgc : sampleAvg;
    const float volumeRaw = cfg.agc ? rawSampleAgc : sampleRaw;
    float my_magnitude = FFT_Magnitude;
    if (cfg.agc) my_magnitude *= multAgc;
    if (volumeSmth < 1) my_magnitude = 0.001f;

    // transmitAudioData(): packet v2
    memset(packet, 0, AR_PACKET_SIZE);
    memcpy(packet, "00002", 6);
    put_f32(packet + 8, volumeRaw);
    put_f32(packet + 12, volumeSmth);
    packet[16] = udpSamplePeak ? 1 : 0;
    udpSamplePeak = false;
    for (int i = 0; i < NCH; i++) packet[18 + i] = fftResult[i] > 254 ? 254 : fftResult[i];
    put_f32(packet + 36, my_magnitude);
    put_f32(packet + 40, FFT_MajorPeak);

    if (out) {
        out->volume_smooth = volumeSmth;
        out->volume_raw = volumeRaw;
        out->fft_magnitude = my_magnitude;
        out->fft_major_peak = FFT_MajorPeak;
        out->agc_gain = multAgc;
        out->peak = packet[16] != 0;
        memcpy(out->fft, fftResult, NCH);
    }
}
