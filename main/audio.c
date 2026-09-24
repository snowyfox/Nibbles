// Microphone capture task.
#include "bsp_board_extra.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors.h"

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
        for (int i = 0; i < AUDIO_FRAME; i++) {
            mono[i] = (raw[i * CHANNELS] + raw[i * CHANNELS + 1]) / (2.0f * 32768.0f);
        }
        audio_analysis_process(&analysis, mono);
        taskENTER_CRITICAL(&lock);
        latest = analysis.out;
        taskEXIT_CRITICAL(&lock);
    }
}

esp_err_t audio_start(void)
{
    audio_analysis_init(&analysis, AUDIO_SAMPLE_RATE);
    latest = analysis.out;
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "codec init failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_in_gain(MIC_GAIN_DB), TAG, "mic gain failed");
    BaseType_t ok = xTaskCreatePinnedToCore(audio_task, "audio", 6144, NULL, 6, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void audio_get(audio_features_t *out)
{
    taskENTER_CRITICAL(&lock);
    *out = latest;
    taskEXIT_CRITICAL(&lock);
}
