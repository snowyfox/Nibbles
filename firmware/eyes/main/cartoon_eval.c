// Temporary evaluation firmware: cycles the cartoon animations every
// CARTOON_EVAL_CYCLE_S seconds, nothing else (no sound, motion, link or
// radio). Built only with -DEYES_CARTOON_EVAL=1:
//   idf.py -C firmware/eyes -B firmware/eyes/build_eval -DEYES_CARTOON_EVAL=1 -p PORT flash
#if EYES_CARTOON_EVAL

#include "board.h"
#include "cartoon.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "render.h"

#define CARTOON_EVAL_CYCLE_S 10

static const char *TAG = "cartoon";

static void *psram_alloc(size_t n)
{
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
}

// The brightness chosen with the BOOT button in the normal firmware.
static int saved_brightness(void)
{
    static const int levels[] = BRIGHTNESS_PRESETS;
    const int n = sizeof(levels) / sizeof(levels[0]);
    int8_t idx = BRIGHTNESS_DEFAULT_INDEX;
    nvs_handle_t h;
    if (nvs_open("nibbles", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i8(h, "bright", &idx);
        nvs_close(h);
    }
    return levels[idx >= 0 && idx < n ? idx : BRIGHTNESS_DEFAULT_INDEX];
}

static void eval_task(void *arg)
{
    const bool mirror = board_side() == NL_SIDE_PORT;
    render_set_brightness(saved_brightness());
    const int64_t start = esp_timer_get_time();
    int64_t last_log = start;
    int shown = -1, frames = 0;
    for (;;) {
        const float t = (esp_timer_get_time() - start) / 1e6f;
        const int anim = (int)(t / CARTOON_EVAL_CYCLE_S) % CARTOON_COUNT;
        if (anim != shown) {
            ESP_LOGI(TAG, "%d/%d %s", anim + 1, CARTOON_COUNT, cartoon_name(anim));
            shown = anim;
        }
        // Each animation starts from its own time 0.
        cartoon_prepare(anim, t - (int)(t / CARTOON_EVAL_CYCLE_S) * CARTOON_EVAL_CYCLE_S, mirror);
        render_frame_rows(cartoon_rows);
        frames++;
        const int64_t now = esp_timer_get_time();
        if (now - last_log >= 1000000) {
            int late;
            float worst_ms, prep_ms;
            render_take_stats(&late, &worst_ms, &prep_ms);
            ESP_LOGI(TAG, "%s: %.1f fps, send %.1f ms max, %d late", cartoon_name(anim),
                     frames * 1e6f / (now - last_log), worst_ms, late);
            frames = 0;
            last_log = now;
        }
        vTaskDelay(1);
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_ERROR_CHECK(render_init());
    if (!cartoon_init(psram_alloc)) {
        ESP_LOGE(TAG, "no memory for the cartoon maps");
        return;
    }
    ESP_LOGI(TAG, "cartoon evaluation: %d animations, %d s each (%s eye)", CARTOON_COUNT, CARTOON_EVAL_CYCLE_S,
             board_side() == NL_SIDE_PORT ? "port" : "starboard");
    xTaskCreatePinnedToCore(eval_task, "cartoon", 6144, NULL, 5, NULL, 1);
}

#endif
