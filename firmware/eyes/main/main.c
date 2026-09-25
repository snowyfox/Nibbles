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
#include "board.h"
#include "eye.h"
#include "link.h"
#include "presets.h"
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
    int preset_index = 0, next_preset = 0;
    int64_t next_swap = last + (int64_t)PRESET_CYCLE_S * 1000000;
    render_set_preset(0);
    ESP_LOGI(TAG, "preset 0: %s", presets[0].name);
    int frames = 0, late_total = 0;
    const nl_role_t role = board_role();
    const nl_side_t side = board_side();
    bool was_linked = false;
    int64_t last_link_log = last;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        float dt = (now - last) / 1e6f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        // The port eye follows the leader while its shared state arrives;
        // otherwise (no cable, or the leader stopped) it runs on its own.
        nl_eye_state_t shared;
        const bool following = role == NL_ROLE_EYE_EARS && link_get_eye_state(&shared, LINK_STATE_MAX_AGE_MS);
        if (following != was_linked) {
            ESP_LOGI(TAG, "%s", following ? "following the leader eye" : "running standalone");
            was_linked = following;
            next_swap = now + (int64_t)PRESET_CYCLE_S * 1000000;
        }

        // Cycle through the visual presets; the eye blinks to hide each change.
        if (!following && now >= next_swap) {
            next_swap += (int64_t)PRESET_CYCLE_S * 1000000;
            next_preset = (preset_index + 1) % preset_count;
            eye_request_swap(&eye);
        }

        audio_features_t a;
        motion_features_t m;
        const bool remote_audio = audio_get(&a);
        motion_get(&m);
        eye_update(&eye, &a, &m, dt);
        if (following) {
            eye_apply_shared(&eye, &shared, side, &m);
            if (shared.preset != preset_index && shared.preset < preset_count) {
                preset_index = shared.preset;
                render_set_preset(preset_index);
                ESP_LOGI(TAG, "preset %d: %s (from the leader)", preset_index, presets[preset_index].name);
            }
        } else if (eye.p.swap_now) {
            preset_index = next_preset;
            render_set_preset(preset_index);
            ESP_LOGI(TAG, "preset %d: %s", preset_index, presets[preset_index].name);
        }
        if (role == NL_ROLE_EYE_LEADER) {
            eye_export_shared(&eye, preset_index, side, &shared);
            link_send_eye_state(&shared);
        }
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
            int late;
            float worst_ms, prep_ms;
            render_take_stats(&late, &worst_ms, &prep_ms);
            link_set_status((uint16_t)(frames / secs * 10.0f + 0.5f), (uint16_t)late);
            late_total += late;
            ESP_LOGI(TAG, "%.1f fps (prep %.1f, send %.1f ms max, %d late) | %.1f dB (floor %.1f, gain %.0f) loud %.2f warm %.2f beats %lu bpm %.0f rhythm %.2f | "
                     "look %+.2f,%+.2f twist %+.0f/s (%.2f) jolt %.2fg dance %.2f (%.2fs) act %.2f | %s lid %.2f hype %.2f",
                     frames / secs, prep_ms, worst_ms, late, a.level_db, a.noise_floor_db, a.gain_db, a.loudness, a.warmth, (unsigned long)a.beat_count,
                     a.beat_period_s > 0 ? 60.0f / a.beat_period_s : 0.0f, a.beat_confidence,
                     m.look_x, m.look_y, m.twist_dps, m.twist_dominance, m.jolt_g, eye.dance, m.dance_period_s, m.activity,
                     eye_state_name(eye.state), eye.p.lid_open, eye.p.hype);
            frames = 0;
            last_log = now;
        }
        if (now - last_link_log >= 5000000) {
            link_stats_t ls;
            link_get_stats(&ls);
            ESP_LOGI(TAG, "link: %s eye (%s), peer %s%s, rx %lu tx %lu frames, crc %lu bad %lu, own echo %lu, audio from %s, late total %d",
                     side == NL_SIDE_PORT ? "port" : "starboard", role == NL_ROLE_EYE_LEADER ? "leader" : "ears",
                     ls.peer_up ? "up" : "down", following ? " (following)" : "",
                     (unsigned long)ls.rx_frames, (unsigned long)ls.tx_frames, (unsigned long)ls.crc_errors,
                     (unsigned long)ls.bad_frames, (unsigned long)ls.own_frames, remote_audio ? "port eye" : "own mic", late_total);
            last_link_log = now;
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
    if (link_start() != ESP_OK) ESP_LOGE(TAG, "eye link unavailable; running standalone");
    if (audio_start() != ESP_OK) ESP_LOGE(TAG, "audio unavailable; eye will not react to sound");
    if (motion_start() != ESP_OK) ESP_LOGE(TAG, "IMU unavailable; eye will not react to motion");

    xTaskCreatePinnedToCore(eye_task, "eye", 6144, NULL, 5, NULL, 1);
}
