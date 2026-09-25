#include <string.h>
// Microphone capture task.
#include "bsp_board_extra.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors.h"
#include "board.h"
#include "link.h"

static const char *TAG = "audio";
#define CHANNELS 2

static audio_analysis_t analysis;
static audio_features_t latest;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static void audio_task(void *arg)
{
    static int16_t raw[AUDIO_FRAME * CHANNELS];
    static float mono[AUDIO_FRAME];
    for (;;) {
        size_t n = 0;
        if (bsp_extra_i2s_read(raw, sizeof(raw), &n, portMAX_DELAY) != ESP_OK || n != sizeof(raw)) {
            ESP_LOGW(TAG, "mic read failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        // While the port eye's analysis is arriving, the leader's own is idle:
        // the mic keeps running so it can take over at once if the link drops.
        nl_audio_t remote;
        if (board_role() == NL_ROLE_EYE_LEADER && link_get_audio(&remote, LINK_STATE_MAX_AGE_MS)) continue;
        for (int i = 0; i < AUDIO_FRAME; i++) {
            mono[i] = (raw[i * CHANNELS] + raw[i * CHANNELS + 1]) / (2.0f * 32768.0f);
        }
        const float old_gain = analysis.out.gain_db;
        audio_analysis_process(&analysis, mono);
        if (analysis.out.gain_db != old_gain) {
            bsp_extra_codec_set_in_gain(analysis.out.gain_db);
            ESP_LOGI(TAG, "mic gain %.0f dB", analysis.out.gain_db);
        }
        taskENTER_CRITICAL(&lock);
        latest = analysis.out;
        taskEXIT_CRITICAL(&lock);
        if (board_role() == NL_ROLE_EYE_EARS) {
            const audio_features_t *o = &analysis.out;
            const nl_audio_t msg = {
                .level_db = o->level_db, .avg_db = o->avg_db, .noise_floor_db = o->noise_floor_db,
                .gain_db = o->gain_db, .loudness = o->loudness, .warmth = o->warmth,
                .beat_period_s = o->beat_period_s, .beat_confidence = o->beat_confidence,
                .beat_count = o->beat_count,
            };
            link_send_audio(&msg);
        }
    }
}

esp_err_t audio_start(void)
{
    audio_analysis_init(&analysis, AUDIO_SAMPLE_RATE, MIC_GAIN_START_DB);
    latest = analysis.out;
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "codec init failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_in_gain(MIC_GAIN_START_DB), TAG, "mic gain failed");
    BaseType_t ok = xTaskCreatePinnedToCore(audio_task, "audio", 6144, NULL, 6, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool audio_get(audio_features_t *out)
{
    nl_audio_t r;
    if (board_role() == NL_ROLE_EYE_LEADER && link_get_audio(&r, LINK_STATE_MAX_AGE_MS)) {
        memset(out, 0, sizeof(*out));
        out->level_db = r.level_db;
        out->avg_db = r.avg_db;
        out->noise_floor_db = r.noise_floor_db;
        out->gain_db = r.gain_db;
        out->loudness = r.loudness;
        out->warmth = r.warmth;
        out->beat_period_s = r.beat_period_s;
        out->beat_confidence = r.beat_confidence;
        out->beat_count = r.beat_count;
        return true;
    }
    taskENTER_CRITICAL(&lock);
    *out = latest;
    taskEXIT_CRITICAL(&lock);
    return false;
}
