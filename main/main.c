// Nibbles: a neon eye for a festival shark totem.
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "config.h"
#include "eye.h"
#include "render.h"
#include "sensors.h"

static const char *TAG = "nibbles";

#define BOOT_BUTTON GPIO_NUM_0
static const int brightness_presets[] = BRIGHTNESS_PRESETS;
#define N_PRESETS ((int)(sizeof(brightness_presets) / sizeof(brightness_presets[0])))

static int load_brightness_index(void)
{
    nvs_handle_t h;
    int8_t idx = BRIGHTNESS_DEFAULT_INDEX;
    if (nvs_open("nibbles", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i8(h, "bright", &idx);
        nvs_close(h);
    }
    return (idx >= 0 && idx < N_PRESETS) ? idx : BRIGHTNESS_DEFAULT_INDEX;
}

static void save_brightness_index(int idx)
{
    nvs_handle_t h;
    if (nvs_open("nibbles", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i8(h, "bright", (int8_t)idx);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Returns true once per press of the BOOT button (debounced across two polls).
static bool button_pressed(void)
{
    static int prev = 1, stable = 1;
    int now = gpio_get_level(BOOT_BUTTON);
    bool pressed = false;
    if (now == prev && now != stable) {
        stable = now;
        pressed = (now == 0);
    }
    prev = now;
    return pressed;
}

static void eye_task(void *arg)
{
    static eye_t eye;
    eye_init(&eye, esp_random());

    int bright_idx = load_brightness_index();
    render_set_brightness(brightness_presets[bright_idx]);
    ESP_LOGI(TAG, "brightness %d%%", brightness_presets[bright_idx]);

    int64_t last = esp_timer_get_time();
    int64_t last_log = last;
    int frames = 0;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        float dt = (now - last) / 1e6f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        audio_features_t a;
        motion_features_t m;
        audio_get(&a);
        motion_get(&m);
        eye_update(&eye, &a, &m, dt);
        render_frame(&eye.p);
        frames++;

        if (button_pressed()) {
            bright_idx = (bright_idx + 1) % N_PRESETS;
            render_set_brightness(brightness_presets[bright_idx]);
            save_brightness_index(bright_idx);
            ESP_LOGI(TAG, "brightness %d%%", brightness_presets[bright_idx]);
        }

        if (now - last_log >= 1000000) {
            float secs = (now - last_log) / 1e6f;
            ESP_LOGI(TAG, "%.1f fps | %.1f dBFS loud %.2f warm %.2f beats %lu bpm %.0f | "
                     "look %+.2f,%+.2f jolt %.2fg dance %.2f (%.2fs) | %s lid %.2f hype %.2f",
                     frames / secs, a.level_db, a.loudness, a.warmth, (unsigned long)a.beat_count,
                     a.beat_period_s > 0 ? 60.0f / a.beat_period_s : 0.0f,
                     m.look_x, m.look_y, m.jolt_g, eye.dance, m.dance_period_s,
                     eye_state_name(eye.state), eye.p.lid_open, eye.p.hype);
            frames = 0;
            last_log = now;
        }
        vTaskDelay(1);  // let the idle task run
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    const gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BOOT_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    ESP_ERROR_CHECK(render_init());
    if (audio_start() != ESP_OK) ESP_LOGE(TAG, "audio unavailable; eye will not react to sound");
    if (motion_start() != ESP_OK) ESP_LOGE(TAG, "IMU unavailable; eye will not react to motion");

    xTaskCreatePinnedToCore(eye_task, "eye", 6144, NULL, 5, NULL, 1);
}
