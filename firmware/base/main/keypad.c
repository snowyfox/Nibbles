#include "keypad.h"

#include <string.h>
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "power.h"

static const char *TAG = "keypad";

typedef struct {
    uint8_t addr;
    uint8_t keys;                 // switches fitted (8 on a column, 3 on the upper keypad)
    ctl_key_t map[8];             // logical key per switch (P0 = switch 1, the first LED)
} board_t;

static const board_t boards[] = KEYPAD_BOARDS;
#define NBOARDS ((int)(sizeof(boards) / sizeof(boards[0])))

static keypad_cb_t cb;
static led_strip_handle_t strip;
static int led_count;
static i2c_master_dev_handle_t devs[NBOARDS];
static volatile uint8_t found;
static uint32_t leds[KEY_COUNT];
static volatile bool leds_dirty = true;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

uint8_t keypad_boards_found(void) { return found; }
int keypad_board_count(void) { return NBOARDS; }

void keypad_set_leds(const uint32_t rgb[KEY_COUNT])
{
    taskENTER_CRITICAL(&lock);
    if (memcmp(leds, rgb, sizeof(leds))) {
        memcpy(leds, rgb, sizeof(leds));
        leds_dirty = true;
    }
    taskEXIT_CRITICAL(&lock);
}

static void show_leds(void)
{
    if (!strip) return;
    uint32_t rgb[KEY_COUNT];
    taskENTER_CRITICAL(&lock);
    memcpy(rgb, leds, sizeof(rgb));
    leds_dirty = false;
    taskEXIT_CRITICAL(&lock);
    // The chain runs through the boards in KEYPAD_BOARDS order, switch 1 first on each.
    int i = 0;
    for (int b = 0; b < NBOARDS; b++) {
        for (int k = 0; k < boards[b].keys; k++, i++) {
            const uint32_t c = boards[b].map[k] ? rgb[boards[b].map[k]] : 0;
            led_strip_set_pixel(strip, i, (c >> 16 & 255) * KEYPAD_LED_LEVEL / 255,
                                (c >> 8 & 255) * KEYPAD_LED_LEVEL / 255, (c & 255) * KEYPAD_LED_LEVEL / 255);
        }
    }
    led_strip_refresh(strip);
}

static void keypad_task(void *arg)
{
    i2c_master_bus_handle_t bus;
    while (!(bus = power_bus())) vTaskDelay(pdMS_TO_TICKS(100));
    for (int b = 0; b < NBOARDS; b++) {
        const i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = boards[b].addr,
                                          .scl_speed_hz = POWER_I2C_HZ };
        i2c_master_bus_add_device(bus, &cfg, &devs[b]);
    }
    uint8_t raw[NBOARDS], stable[NBOARDS];  // input register, bits low = pressed
    memset(raw, 0xff, sizeof(raw));
    memset(stable, 0xff, sizeof(stable));
    int64_t next_probe = 0, next_leds = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(KEYPAD_POLL_MS));
        const int64_t now = esp_timer_get_time();
        if (now >= next_probe) {   // boards plugged in later are picked up within 2 s
            next_probe = now + 2000000;
            for (int b = 0; b < NBOARDS; b++) {
                if (found & 1 << b) continue;
                if (i2c_master_probe(bus, boards[b].addr, 10) == ESP_OK) {
                    found |= 1 << b;
                    ESP_LOGI(TAG, "keypad board 0x%02x found (%d keys)", boards[b].addr, boards[b].keys);
                    leds_dirty = true;
                }
            }
        }
        for (int b = 0; b < NBOARDS; b++) {
            if (!(found & 1 << b)) continue;
            uint8_t reg = 0, in;
            if (i2c_master_transmit_receive(devs[b], &reg, 1, &in, 1, 10) != ESP_OK) {
                ESP_LOGW(TAG, "keypad board 0x%02x lost", boards[b].addr);
                found &= ~(1 << b);
                // whatever it had down comes up
                for (int k = 0; k < boards[b].keys; k++)
                    if (!(stable[b] & 1 << k) && boards[b].map[k] && cb) cb(boards[b].map[k], false);
                raw[b] = stable[b] = 0xff;
                continue;
            }
            // Debounce: a change counts once two reads in a row agree.
            const uint8_t settled = ~(in ^ raw[b]);           // bits that read the same twice
            const uint8_t changed = (stable[b] ^ in) & settled;
            raw[b] = in;
            if (!changed) continue;
            for (int k = 0; k < boards[b].keys; k++) {
                if (!(changed & 1 << k)) continue;
                const bool down = !(in & 1 << k);
                if (boards[b].map[k] && cb) cb(boards[b].map[k], down);
            }
            stable[b] ^= changed;
        }
        if (leds_dirty || now >= next_leds) {   // also refreshed now and then, in case a board was replugged
            next_leds = now + 1000000;
            show_leds();
        }
    }
}

esp_err_t keypad_start(keypad_cb_t on_key)
{
    cb = on_key;
    for (int b = 0; b < NBOARDS; b++) led_count += boards[b].keys;
    const led_strip_config_t sc = {
        .strip_gpio_num = PIN_KEY_LEDS,
        .max_leds = led_count,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rc = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10 * 1000 * 1000 };
    if (led_strip_new_rmt_device(&sc, &rc, &strip) != ESP_OK) {
        ESP_LOGE(TAG, "no LED driver; keys work without their lights");
        strip = NULL;
    }
    return xTaskCreate(keypad_task, "keypad", 4096, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
