#include "display.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_axs15231b.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "display";

// Waveshare ESP32-S3-Touch-LCD-3.49 panel wiring.
#define LCD_HOST       SPI3_HOST
#define PIN_CS         9
#define PIN_PCLK       10
#define PIN_D0         11
#define PIN_D1         12
#define PIN_D2         13
#define PIN_D3         14
#define PIN_RST        21
#define PIN_BL         8
#define PANEL_W        172     // native (portrait) size
#define PANEL_H        640
#define CHUNK_ROWS     64      // rows per QSPI transfer (the panel takes whole frames, in order)
#define FRAME_BYTES    (PANEL_W * PANEL_H * 2)
#define CHUNK_BYTES    (PANEL_W * CHUNK_ROWS * 2)

#define TOUCH_SDA      17
#define TOUCH_SCL      18
#define TOUCH_ADDR     0x3B

#define UPDATE_MS      250

static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t chunk_done;
static uint16_t *chunk_buf;   // DMA-capable, internal RAM
static uint8_t *rotated;      // the frame in the panel's native orientation

// Screenshots for checking the layout from a computer (serial "shot").
static volatile bool shot_wanted;
static uint8_t *shot_buf;
static int shot_w, shot_h;
static SemaphoreHandle_t shot_ready;

static i2c_master_dev_handle_t touch_dev;
static display_action_cb_t action_cb;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static display_status_t status;
static char event_text[64];
static bool event_new;

// ------------------------------------------------------------------ panel

static bool on_chunk_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(chunk_done, &woken);
    return woken == pdTRUE;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    // Full-frame render mode: px holds the whole (landscape) screen and LVGL
    // keeps it between frames, redrawing only what changed, so it must not be
    // modified here. Rotate into the panel's orientation, then byte-swap that.
    const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
    if (shot_wanted && shot_buf) {
        memcpy(shot_buf, px, w * h * 2);
        shot_w = w;
        shot_h = h;
        shot_wanted = false;
        xSemaphoreGive(shot_ready);
    }
    const lv_color_format_t cf = lv_display_get_color_format(disp);
    lv_draw_sw_rotate(px, rotated, w, h, lv_draw_buf_width_to_stride(w, cf), lv_draw_buf_width_to_stride(PANEL_W, cf),
                      lv_display_get_rotation(disp), cf);
    lv_draw_sw_rgb565_swap(rotated, PANEL_W * PANEL_H);
    const uint8_t *src = rotated;
    for (int y = 0; y < PANEL_H; y += CHUNK_ROWS, src += CHUNK_BYTES) {
        memcpy(chunk_buf, src, CHUNK_BYTES);
        esp_lcd_panel_draw_bitmap(panel, 0, y, PANEL_W, y + CHUNK_ROWS, chunk_buf);
        xSemaphoreTake(chunk_done, portMAX_DELAY);
    }
    lv_display_flush_ready(disp);
}

static void backlight_init(int percent)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_3,
        .freq_hz = 50000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t ch = {
        .gpio_num = PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_3,
        .duty = 255 * percent / 100,
    };
    ledc_timer_config(&timer);
    ledc_channel_config(&ch);
}

static esp_err_t panel_init(void)
{
    chunk_done = xSemaphoreCreateBinary();
    chunk_buf = heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    rotated = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(chunk_done && chunk_buf && rotated, ESP_ERR_NO_MEM, TAG, "no memory");

    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_PCLK,
        .data0_io_num = PIN_D0,
        .data1_io_num = PIN_D1,
        .data2_io_num = PIN_D2,
        .data3_io_num = PIN_D3,
        .max_transfer_sz = CHUNK_BYTES,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_handle_t io;
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = -1,
        .spi_mode = 3,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_chunk_done,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags.quad_mode = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &io), TAG, "panel io");

    const axs15231b_lcd_init_cmd_t init_cmds[] = {  // used by esp_lcd_panel_init below
        { 0x11, (uint8_t[]){ 0x00 }, 0, 100 },  // sleep out
        { 0x29, (uint8_t[]){ 0x00 }, 0, 100 },  // display on
    };
    axs15231b_vendor_config_t vendor = {
        .init_cmds = init_cmds,
        .init_cmds_size = sizeof(init_cmds) / sizeof(init_cmds[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = -1,  // reset by hand below, with the timing the panel needs
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_axs15231b(io, &dev, &panel), TAG, "panel");

    const gpio_config_t rst = { .pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst);
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    return esp_lcd_panel_init(panel);
}

// ------------------------------------------------------------------ touch

static esp_err_t touch_init(void)
{
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = TOUCH_SCL,
        .sda_io_num = TOUCH_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "touch i2c bus");
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDR,
        .scl_speed_hz = 300000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &touch_dev), TAG, "touch device");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, TOUCH_ADDR, 50), TAG, "touch controller not answering");
    ESP_LOGI(TAG, "touch controller found");
    return ESP_OK;
}

// AXS15231B touch: one read command, returns the number of points and the
// first point in the panel's native (portrait) frame. LVGL rotates it.
static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static const uint8_t cmd[11] = { 0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00 };
    uint8_t buf[14] = { 0 };
    data->state = LV_INDEV_STATE_RELEASED;
    if (i2c_master_transmit_receive(touch_dev, cmd, sizeof(cmd), buf, sizeof(buf), 20) != ESP_OK) return;
    if (buf[1] == 0 || buf[1] > 4) return;
    int raw_x = ((buf[2] & 0x0f) << 8) | buf[3];  // along the long side
    int raw_y = ((buf[4] & 0x0f) << 8) | buf[5];  // along the short side
    if (raw_x > PANEL_H) raw_x = PANEL_H;
    if (raw_y > PANEL_W - 1) raw_y = PANEL_W - 1;
    data->point.x = raw_y;
    data->point.y = PANEL_H - raw_x;
    if (data->point.y > PANEL_H - 1) data->point.y = PANEL_H - 1;
    data->state = LV_INDEV_STATE_PRESSED;
}

static void on_event(lv_event_t *ev)
{
    const display_action_t a = (display_action_t)(intptr_t)lv_event_get_user_data(ev);
    const lv_event_code_t code = lv_event_get_code(ev);
    if (!action_cb) return;
    if (code == LV_EVENT_CLICKED) action_cb(a, true);
    else if (code == LV_EVENT_PRESSED) action_cb(a, true);
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) action_cb(a, false);
}

// ------------------------------------------------------------------ UI

#define COL_BG      0x000000
#define COL_CARD    0x14161c
#define COL_TITLE   0x7f8794
#define COL_TEXT    0xe8ecf2
#define COL_DIM     0x4a505a
#define COL_OK      0x3ddc84
#define COL_WARN    0xffb020
#define COL_BAD     0xff4d4d
#define COL_EYES    0x36c8ff
#define COL_WLED    0xff7ad9

typedef struct {
    lv_obj_t *card, *title, *big, *line1, *line2, *bar;
} card_t;

static card_t eyes_card, wled_card, radio_card;
static lv_obj_t *event_label, *screen, *pad_flash, *pad_black;

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void make_card(card_t *c, lv_obj_t *parent, const char *title, uint32_t accent, bool with_bar)
{
    c->card = lv_obj_create(parent);
    lv_obj_remove_flag(c->card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(c->card, 204, 136);
    lv_obj_set_style_bg_color(c->card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_width(c->card, 0, 0);
    lv_obj_set_style_radius(c->card, 10, 0);
    lv_obj_set_style_pad_all(c->card, 8, 0);
    lv_obj_set_flex_flow(c->card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c->card, 2, 0);

    c->title = label(c->card, &lv_font_montserrat_14, accent);
    lv_label_set_text(c->title, title);
    c->big = label(c->card, &lv_font_montserrat_28, COL_TEXT);
    c->line1 = label(c->card, &lv_font_montserrat_16, COL_TEXT);
    c->line2 = label(c->card, &lv_font_montserrat_14, COL_TITLE);
    c->bar = NULL;
    if (with_bar) {
        c->bar = lv_bar_create(c->card);
        lv_obj_set_size(c->bar, LV_PCT(100), 6);
        lv_bar_set_range(c->bar, 0, 255);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(COL_DIM), LV_PART_MAIN);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(accent), LV_PART_INDICATOR);
        lv_obj_set_style_margin_top(c->bar, 4, 0);
    }
}

static lv_obj_t *make_pad(const char *text, uint32_t color, display_action_t action, int x_ofs)
{
    lv_obj_t *b = lv_button_create(screen);
    lv_obj_set_size(b, 96, 26);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, x_ofs, -3);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = label(b, &lv_font_montserrat_14, COL_TEXT);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, on_event, LV_EVENT_PRESSED, (void *)(intptr_t)action);
    lv_obj_add_event_cb(b, on_event, LV_EVENT_RELEASED, (void *)(intptr_t)action);
    lv_obj_add_event_cb(b, on_event, LV_EVENT_PRESS_LOST, (void *)(intptr_t)action);
    return b;
}

static void card_stale(card_t *c, bool stale)
{
    const uint32_t col = stale ? COL_DIM : COL_TEXT;
    lv_obj_set_style_text_color(c->big, lv_color_hex(col), 0);
    lv_obj_set_style_text_color(c->line1, lv_color_hex(col), 0);
}

static void ui_build(void)
{
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_color(screen, lv_color_hex(COL_WARN), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, 640, 140);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_card(&eyes_card, row, "EYES", COL_EYES, true);
    make_card(&wled_card, row, "WLED", COL_WLED, true);
    make_card(&radio_card, row, "RADIO", COL_OK, false);

    event_label = label(screen, &lv_font_montserrat_16, COL_TITLE);
    lv_obj_align(event_label, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_label_set_text(event_label, "Nibbles base");

    // Tapping a card steps that side's preset.
    lv_obj_add_flag(eyes_card.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(eyes_card.card, on_event, LV_EVENT_CLICKED, (void *)(intptr_t)DISPLAY_TAP_EYES);
    lv_obj_add_flag(wled_card.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wled_card.card, on_event, LV_EVENT_CLICKED, (void *)(intptr_t)DISPLAY_TAP_WLED);
    lv_obj_set_style_bg_color(eyes_card.card, lv_color_hex(0x232733), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(wled_card.card, lv_color_hex(0x232733), LV_STATE_PRESSED);

    // Bump pads: held while touched.
    pad_black = make_pad("BLACKOUT", 0x5a5f6a, DISPLAY_PAD_BLACKOUT, -8);
    pad_flash = make_pad("FLASH", COL_WARN, DISPLAY_PAD_FLASH, -8 - 104);
}

static const char *eye_state(uint8_t s)
{
    static const char *names[] = { "awake", "drowsy", "asleep", "waking" };
    return s < 4 ? names[s] : "?";
}

static const char *bump_name(uint8_t a)
{
    return a == NL_BUMP_FLASH ? "FLASH" : a == NL_BUMP_BLACKOUT ? "BLACKOUT" : a == NL_BUMP_PRESET ? "PRESET" : "";
}

static void ui_update(const display_status_t *s)
{
    // Eyes
    card_stale(&eyes_card, !s->eyes_fresh);
    if (s->eyes_fresh) {
        const nl_eye_telemetry_t *e = &s->eyes;
        lv_label_set_text_fmt(eyes_card.big, "Preset %d/%d", e->preset + 1, e->preset_count);
        lv_label_set_text_fmt(eyes_card.line1, "%s  %s", eye_state(e->state), e->linked ? "linked" : "NOT linked");
        lv_obj_set_style_text_color(eyes_card.line1, lv_color_hex(e->linked ? COL_TEXT : COL_WARN), 0);
        // LVGL's printf has no floats.
        lv_label_set_text_fmt(eyes_card.line2, "%d.%d | %d.%d fps   %d bpm", e->fps_x10[0] / 10, e->fps_x10[0] % 10,
                              e->fps_x10[1] / 10, e->fps_x10[1] % 10, (int)(e->tempo_bpm + 0.5f));
        lv_bar_set_value(eyes_card.bar, (int32_t)(e->hype * 255.0f), LV_ANIM_OFF);
    } else {
        lv_label_set_text(eyes_card.big, "--");
        lv_label_set_text(eyes_card.line1, "not heard");
        lv_label_set_text(eyes_card.line2, "");
        lv_bar_set_value(eyes_card.bar, 0, LV_ANIM_OFF);
    }

    // WLED
    card_stale(&wled_card, !s->wled_fresh);
    if (s->wled_fresh) {
        const nl_wled_telemetry_t *w = &s->wled;
        if (!w->on) lv_label_set_text(wled_card.big, "OFF");
        else if (w->preset) lv_label_set_text_fmt(wled_card.big, "Preset %d", w->preset);
        else lv_label_set_text(wled_card.big, "No preset");
        lv_label_set_text_fmt(wled_card.line1, "bri %d  fx %d  pal %d", w->bri, w->fx, w->palette);
        lv_obj_set_style_text_color(wled_card.line1, lv_color_hex(COL_TEXT), 0);
        lv_label_set_text_fmt(wled_card.line2, "%d fps   %d LEDs", w->fps, w->leds);
        lv_bar_set_value(wled_card.bar, w->on ? w->bri : 0, LV_ANIM_OFF);
    } else {
        lv_label_set_text(wled_card.big, "--");
        lv_label_set_text(wled_card.line1, "not heard");
        lv_label_set_text(wled_card.line2, "");
        lv_bar_set_value(wled_card.bar, 0, LV_ANIM_OFF);
    }

    // Radio
    lv_label_set_text_fmt(radio_card.big, "Ch %d", s->channel);
    lv_label_set_text(radio_card.line1, s->anchoring ? "anchoring" : s->locked ? "locked" : "scanning");
    lv_obj_set_style_text_color(radio_card.line1, lv_color_hex(s->anchoring ? COL_WARN : s->locked ? COL_OK : COL_BAD), 0);
    lv_label_set_text_fmt(radio_card.line2, "rx %lu  tx %lu  fail %lu", (unsigned long)s->rx, (unsigned long)s->tx,
                          (unsigned long)s->tx_fail);

    // A held bump outlines the whole screen.
    lv_obj_set_style_border_width(screen, s->bump_action ? 4 : 0, 0);
    if (s->bump_action) {
        lv_obj_set_style_text_color(event_label, lv_color_hex(COL_WARN), 0);
        lv_label_set_text_fmt(event_label, "BUMP  %s", bump_name(s->bump_action));
    }
}

static uint32_t tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void display_task(void *arg)
{
    int64_t next_update = 0;
    uint8_t last_bump = 0;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        if (now >= next_update) {
            next_update = now + UPDATE_MS * 1000LL;
            display_status_t s;
            char ev[sizeof(event_text)];
            bool new_event;
            taskENTER_CRITICAL(&lock);
            s = status;
            new_event = event_new;
            event_new = false;
            memcpy(ev, event_text, sizeof(ev));
            taskEXIT_CRITICAL(&lock);
            ui_update(&s);
            if (new_event || (last_bump && !s.bump_action)) {
                lv_obj_set_style_text_color(event_label, lv_color_hex(COL_TITLE), 0);
                lv_label_set_text(event_label, ev);
            }
            last_bump = s.bump_action;
        }
        if (shot_wanted) lv_obj_invalidate(screen);
        const uint32_t wait = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(wait < 10 ? 10 : wait > UPDATE_MS ? UPDATE_MS : wait));
    }
}

esp_err_t display_start(display_action_cb_t on_action)
{
    action_cb = on_action;
    backlight_init(DISPLAY_BACKLIGHT_PCT);
    ESP_RETURN_ON_ERROR(panel_init(), TAG, "panel init failed");

    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *disp = lv_display_create(PANEL_W, PANEL_H);
    // Rotate first: the buffer's stride comes from the (rotated) width.
    lv_display_set_rotation(disp, DISPLAY_ROTATION);
    uint8_t *buf = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "no memory for the frame");
    lv_display_set_buffers(disp, buf, NULL, FRAME_BYTES, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    ui_build();
    if (touch_init() == ESP_OK) {
        lv_indev_t *touch = lv_indev_create();
        lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch, touch_read);
    } else {
        ESP_LOGW(TAG, "no touch");
    }

    BaseType_t ok = xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 2, NULL, 1);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void display_screenshot(void)
{
    if (!shot_ready) shot_ready = xSemaphoreCreateBinary();
    if (!shot_buf) shot_buf = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
    if (!shot_ready || !shot_buf) return;
    xSemaphoreTake(shot_ready, 0);
    shot_wanted = true;
    if (xSemaphoreTake(shot_ready, pdMS_TO_TICKS(2000)) != pdTRUE) {
        shot_wanted = false;
        printf("SHOT failed\n");
        return;
    }
    // Little-endian RGB565, row by row, base64 in 76-character lines.
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t n = (size_t)shot_w * shot_h * 2;
    printf("SHOT %d %d rgb565le\n", shot_w, shot_h);
    char line[80];
    size_t k = 0;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = shot_buf[i] << 16 | (i + 1 < n ? shot_buf[i + 1] << 8 : 0) | (i + 2 < n ? shot_buf[i + 2] : 0);
        line[k++] = b64[(v >> 18) & 63];
        line[k++] = b64[(v >> 12) & 63];
        line[k++] = i + 1 < n ? b64[(v >> 6) & 63] : '=';
        line[k++] = i + 2 < n ? b64[v & 63] : '=';
        if (k >= 76 || i + 3 >= n) {
            line[k] = 0;
            printf("%s\n", line);
            k = 0;
        }
    }
    printf("SHOT END\n");
}

void display_update(const display_status_t *s)
{
    taskENTER_CRITICAL(&lock);
    status = *s;
    taskEXIT_CRITICAL(&lock);
}

void display_event(const char *fmt, ...)
{
    char text[sizeof(event_text)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    taskENTER_CRITICAL(&lock);
    memcpy(event_text, text, sizeof(text));
    event_new = true;
    taskEXIT_CRITICAL(&lock);
}
