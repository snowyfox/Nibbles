// Nibbles: a neon eye for a festival shark totem.
#include <stdio.h>
#include <string.h>
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
#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "nibbles_espnow.h"
#include "presets.h"
#include "render.h"
#include "sensors.h"

static const char *TAG = "nibbles";

#define BOOT_BUTTON GPIO_NUM_0
static const int brightness_presets[] = BRIGHTNESS_PRESETS;
#define N_PRESETS ((int)(sizeof(brightness_presets) / sizeof(brightness_presets[0])))

// Preset commands and bumps from the radio, handed to the eye task.
static QueueHandle_t radio_cmds, radio_bumps;

typedef struct {
    nl_bump_t bump;
    int64_t rx_us;  // when it arrived, to measure how soon it shows
} bump_item_t;

static void on_radio(const uint8_t mac[6], const nl_radio_hdr_t *h, const uint8_t *pl, size_t len)
{
    if (h->type == NL_MSG_AUDIO && len == sizeof(nl_audio_t) && h->role == NL_ROLE_WLED) {
        nl_audio_t a;
        memcpy(&a, pl, sizeof(a));
        audio_set_radio(&a);
        return;
    }
    if (h->type == NL_MSG_BUMP && len == sizeof(nl_bump_t)) {
        bump_item_t it = { .rx_us = esp_timer_get_time() };
        memcpy(&it.bump, pl, sizeof(it.bump));
        if (it.bump.target & NL_TARGET_EYES) xQueueSend(radio_bumps, &it, 0);
        return;
    }
    if (h->type != NL_MSG_CMD || len != sizeof(nl_cmd_t)) return;
    nl_cmd_t cmd;
    memcpy(&cmd, pl, sizeof(cmd));
    // Retries reuse the id: acknowledge again, but apply once.
    static uint16_t last_id;
    static uint8_t last_mac[6];
    const bool repeat = cmd.id == last_id && memcmp(mac, last_mac, 6) == 0;
    uint8_t status = NL_ACK_OK;
    if (cmd.target != NL_TARGET_EYES) status = NL_ACK_UNSUPPORTED;
    else if (cmd.op == NL_OP_PRESET_SET && (cmd.arg < 0 || cmd.arg >= preset_count)) status = NL_ACK_BAD_ARG;
    else if (cmd.op == NL_OP_BRIGHTNESS_SET && (cmd.arg < 0 || cmd.arg > 255)) status = NL_ACK_BAD_ARG;
    else if (cmd.op == NL_OP_REACTIVITY && (cmd.arg < NL_REACT_PRESET || cmd.arg > NL_REACT_PEAKS)) status = NL_ACK_BAD_ARG;
    else if (cmd.op < NL_OP_PRESET_SET || cmd.op > NL_OP_REACTIVITY) status = NL_ACK_UNSUPPORTED;
    else if (!repeat) xQueueSend(radio_cmds, &cmd, 0);
    last_id = cmd.id;
    memcpy(last_mac, mac, 6);
    nl_espnow_ack(mac, cmd.id, status);
}

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
    int bright_pct = brightness_presets[bright_idx];  // can also be set to any level over the radio
    render_set_brightness(bright_pct);
    ESP_LOGI(TAG, "brightness %d%%", bright_pct);

    int64_t last = esp_timer_get_time();
    int64_t last_log = last;
    int preset_index = 0, next_preset = 0;
    int rendered = 0, bump_preset = -1;  // a preset bump shows bump_preset while held
    nl_bump_rx_t bumps;
    nl_bump_rx_init(&bumps);
    int64_t bump_rx_us = 0;
    // Presets change every PRESET_CYCLE_S after boot; choosing one from the base
    // stops that (the base can turn it back on).
    bool auto_cycle = true;
    nl_react_t react = NL_REACT_PRESET;  // sound reactivity override from the base
    int64_t next_swap = last + (int64_t)PRESET_CYCLE_S * 1000000;
    render_set_preset(0);
    ESP_LOGI(TAG, "preset 0: %s", presets[0].name);
    int frames = 0, late_total = 0;
    const nl_role_t role = board_role();
    const nl_side_t side = board_side();
    bool was_linked = false;
    int64_t last_link_log = last, next_telemetry = last;
    uint16_t fps_x10 = 0, late_second = 0;
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

        // Brightness: the port eye matches the leader while linked.
        if (following && shared.brightness != bright_pct && shared.brightness <= 100) {
            bright_pct = shared.brightness;
            render_set_brightness(bright_pct);
            ESP_LOGI(TAG, "brightness %d%% (from the leader)", bright_pct);
        }

        // A command from the radio. A preset chosen this way holds for a full cycle.
        nl_cmd_t cmd;
        if (!following && radio_cmds && xQueueReceive(radio_cmds, &cmd, 0) == pdTRUE) {
            if (cmd.op == NL_OP_REACTIVITY) {
                react = (nl_react_t)cmd.arg;
                ESP_LOGI(TAG, "radio command: react to %s", react == NL_REACT_BEATS ? "beats" : react == NL_REACT_PEAKS ? "peaks" : "each preset's choice");
            } else if (cmd.op == NL_OP_AUTO_CYCLE) {
                auto_cycle = cmd.arg != 0;
                next_swap = now + (int64_t)PRESET_CYCLE_S * 1000000;
                ESP_LOGI(TAG, "radio command: presets %s", auto_cycle ? "cycle" : "held");
            } else if (cmd.op == NL_OP_BRIGHTNESS_SET || cmd.op == NL_OP_BRIGHTNESS_STEP) {
                if (cmd.op == NL_OP_BRIGHTNESS_SET) {
                    bright_pct = (cmd.arg * 100 + 127) / 255;  // not saved: the presets are what BOOT cycles
                } else {
                    // To the next brightness preset above (or below) the current level.
                    int i = cmd.arg > 0 ? 0 : N_PRESETS - 1;
                    if (cmd.arg > 0) while (i < N_PRESETS - 1 && brightness_presets[i] <= bright_pct) i++;
                    else while (i > 0 && brightness_presets[i] >= bright_pct) i--;
                    bright_idx = i;
                    bright_pct = brightness_presets[bright_idx];
                    save_brightness_index(bright_idx);
                }
                render_set_brightness(bright_pct);
                ESP_LOGI(TAG, "radio command: brightness %d%%", bright_pct);
            } else {
                const int base = eye.swap_pending ? next_preset : preset_index;
                if (cmd.op == NL_OP_PRESET_SET) next_preset = cmd.arg;
                else if (cmd.op == NL_OP_PRESET_NEXT) next_preset = (base + 1) % preset_count;
                else next_preset = (base + preset_count - 1) % preset_count;
                eye_request_swap(&eye);
                next_swap = now + (int64_t)PRESET_CYCLE_S * 1000000;
                if (auto_cycle) ESP_LOGI(TAG, "presets held (chosen from the base)");
                auto_cycle = false;  // a preset picked by hand stays until the next pick
                ESP_LOGI(TAG, "radio command: preset %d next", next_preset);
            }
        }

        // Bumps: flash and blackout are drawn by the eye at once; a preset bump
        // blinks to its preset and blinks back on release (see swap_now below).
        bump_item_t bmsg;
        const uint32_t now_ms = (uint32_t)(now / 1000);
        nl_bump_event_t ev = NL_BUMP_EV_NONE;
        if (radio_bumps && xQueueReceive(radio_bumps, &bmsg, 0) == pdTRUE) ev = nl_bump_rx_message(&bumps, &bmsg.bump, now_ms);
        if (ev == NL_BUMP_EV_BEGIN || ev == NL_BUMP_EV_REPLACE) bump_rx_us = bmsg.rx_us;
        if (ev == NL_BUMP_EV_NONE) ev = nl_bump_rx_tick(&bumps, now_ms);
        if (ev == NL_BUMP_EV_END || ev == NL_BUMP_EV_REPLACE) {
            eye_set_bump(&eye, 0);
            bump_preset = -1;
        }
        if (ev == NL_BUMP_EV_BEGIN || ev == NL_BUMP_EV_REPLACE) {
            eye_set_bump(&eye, bumps.bump.action);
            // Preset bumps are 1-based (the same number means the same on WLED).
            if (bumps.bump.action == NL_BUMP_PRESET && bumps.bump.arg >= 1 && bumps.bump.arg <= preset_count)
                bump_preset = bumps.bump.arg - 1;
        }
        if (ev != NL_BUMP_EV_NONE) ESP_LOGI(TAG, "bump %s (action %d)", ev == NL_BUMP_EV_END ? "end" : "begin", bumps.bump.action);

        // Cycle through the visual presets; the eye blinks to hide each change.
        if (!following && auto_cycle && now >= next_swap) {
            next_swap += (int64_t)PRESET_CYCLE_S * 1000000;
            next_preset = (preset_index + 1) % preset_count;
            eye_request_swap(&eye);
        }

        audio_features_t a;
        motion_features_t m;
        const audio_source_t audio_src = audio_get(&a);
        motion_get(&m);
        eye_update(&eye, &a, &m, dt);
        if (following) {
            eye_apply_shared(&eye, &shared, side, &m);
            if (shared.preset != preset_index && shared.preset < preset_count) {
                preset_index = shared.preset;
                ESP_LOGI(TAG, "preset %d: %s (from the leader)", preset_index, presets[preset_index].name);
            }
            if (preset_index != rendered) {  // the leader's lids are shut for it
                rendered = preset_index;
                render_set_preset(rendered);
            }
        } else {
            // What the eye should show: a held preset bump, else its own preset.
            // Every change hides behind a swap blink and happens while the lids
            // are shut.
            const int wanted = bump_preset >= 0 ? bump_preset : next_preset;
            if (wanted != rendered && !eye.swap_pending && !eye.p.swap_now) eye_request_swap(&eye);
            if (eye.p.swap_now) {
                if (next_preset != preset_index) {
                    preset_index = next_preset;
                    ESP_LOGI(TAG, "preset %d: %s", preset_index, presets[preset_index].name);
                }
                const int show = bump_preset >= 0 ? bump_preset : preset_index;
                if (show != rendered) {
                    rendered = show;
                    render_set_preset(rendered);
                    if (bump_preset >= 0) ESP_LOGI(TAG, "bump preset %d: %s", rendered, presets[rendered].name);
                }
            }
        }
        const bool peaks = react == NL_REACT_PRESET ? presets[rendered].peak_beats : react == NL_REACT_PEAKS;
        eye_set_peak_beats(&eye, peaks);  // the preset on screen decides, unless the base overrides
        if (role == NL_ROLE_EYE_LEADER) {
            eye_export_shared(&eye, rendered, side, &shared);
            shared.brightness = (uint8_t)bright_pct;
            link_send_eye_state(&shared);
        }
        render_frame(&eye.p);
        frames++;
        if (bump_rx_us && (bumps.bump.action != NL_BUMP_PRESET || bump_preset < 0 || rendered == bump_preset)) {
            ESP_LOGI(TAG, "bump shown %.1f ms after it arrived", (esp_timer_get_time() - bump_rx_us) / 1000.0f);
            bump_rx_us = 0;
        }

        if (radio_cmds && now >= next_telemetry) {
            next_telemetry = now + TELEMETRY_MS * 1000LL;
            link_stats_t ls;
            link_get_stats(&ls);
            nl_eye_telemetry_t t = {
                .preset = (uint8_t)preset_index,
                .preset_count = (uint8_t)preset_count,
                .state = (uint8_t)eye.state,
                .linked = ls.peer_up,
                .fps_x10 = { fps_x10, ls.peer_up ? ls.peer_fps_x10 : 0 },
                .late_frames = { late_second, ls.peer_up ? ls.peer_late : 0 },
                .tempo_bpm = eye.p.tempo_bpm,
                .level_db = a.level_db,
                .hype = eye.p.hype,
                .brightness = (uint8_t)bright_pct,
                .flags = (uint8_t)((auto_cycle ? NL_EYE_AUTO_CYCLE : 0) | NL_EYE_REACT_FLAGS(react) |
                                   (eye.peak_beats ? NL_EYE_PEAKS_NOW : 0)),
            };
            strlcpy(t.name, presets[rendered].name, sizeof(t.name));
            nl_espnow_broadcast(NL_MSG_EYE_TELEMETRY, &t, sizeof(t));
        }

        if (button_pressed()) {
            if (following) {
                ESP_LOGI(TAG, "brightness follows the leader eye; use its BOOT button");
            } else {
                bright_idx = (bright_idx + 1) % N_PRESETS;
                bright_pct = brightness_presets[bright_idx];
                render_set_brightness(bright_pct);
                save_brightness_index(bright_idx);
                ESP_LOGI(TAG, "brightness %d%%", bright_pct);
            }
        }

        if (now - last_log >= 1000000) {
            float secs = (now - last_log) / 1e6f;
            int late;
            float worst_ms, prep_ms;
            render_take_stats(&late, &worst_ms, &prep_ms);
            fps_x10 = (uint16_t)(frames / secs * 10.0f + 0.5f);
            late_second = (uint16_t)late;
            link_set_status(fps_x10, late_second);
            if (radio_cmds) nl_espnow_set_status(fps_x10, late_second);
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
                     (unsigned long)ls.bad_frames, (unsigned long)ls.own_frames, audio_source_name(audio_src), late_total);
            if (radio_cmds) {
                nl_espnow_stats_t rs;
                nl_espnow_get_stats(&rs);
                ESP_LOGI(TAG, "radio: channel %d %s, rx %lu tx %lu fail %lu dropped %lu, %lu locks, internal heap free %u",
                         rs.channel, rs.locked ? "locked" : "scanning", (unsigned long)rs.rx, (unsigned long)rs.tx,
                         (unsigned long)rs.tx_fail, (unsigned long)rs.dropped, (unsigned long)rs.locks,
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            }
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
    if (EYES_RADIO && board_role() == NL_ROLE_EYE_LEADER) {
        radio_cmds = xQueueCreate(4, sizeof(nl_cmd_t));
        radio_bumps = xQueueCreate(8, sizeof(bump_item_t));
        const nl_espnow_config_t rcfg = {
            .role = NL_ROLE_EYE_LEADER,
            .side = board_side(),
            .fw = NIBBLES_FW_BUILD,
            .anchor = false,
            .anchor_channel = RADIO_START_CHANNEL,
            .handler = on_radio,
            .core = 0,
        };
        if (nl_espnow_start(&rcfg) != ESP_OK) {
            ESP_LOGE(TAG, "radio unavailable");
            radio_cmds = NULL;
        }
    }
    if (audio_start() != ESP_OK) ESP_LOGE(TAG, "audio unavailable; eye will not react to sound");
    if (motion_start() != ESP_OK) ESP_LOGE(TAG, "IMU unavailable; eye will not react to motion");

    xTaskCreatePinnedToCore(eye_task, "eye", 6144, NULL, 5, NULL, 1);
}
