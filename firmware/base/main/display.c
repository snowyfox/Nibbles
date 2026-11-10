#include "display.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "nibbles_link.h"
#include "power_math.h"
#include "fin_pos.h"
#include "eye_thumb.h"
#include "shark_pos.h"
#include "wled_palettes.h"

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
static uint16_t *chunk_bufs[2];   // DMA-capable, internal RAM: one fills while the other goes out
static uint8_t *rotated;      // the frame in the panel's native orientation (landscape only)

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

// For the log line: time from LVGL starting a frame to it being sent, and in ui_update.
static int64_t refr_start_us, refr_us;
static void on_refr(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) refr_start_us = esp_timer_get_time();
    else refr_us += esp_timer_get_time() - refr_start_us;
}
static int64_t ui_us;
static const uint16_t *pv_row(int y, int *x0, int *w);   // the palette viewer's list, put in each frame
static uint32_t flush_count;
static int64_t flush_us;   // time spent sending frames, for the log

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    // px is the whole screen, kept by LVGL between frames, so it must not be
    // modified here. Portrait renders in direct mode: LVGL redraws only what
    // changed (calling this once per changed area) and the panel, which takes
    // whole frames, gets the frame after the last one. Landscape renders in full
    // mode (LVGL can't rotate in direct mode): every frame is drawn whole.
    if (!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }
    (void)area;
    const int64_t t0 = esp_timer_get_time();
    const int32_t w = lv_display_get_horizontal_resolution(disp), h = lv_display_get_vertical_resolution(disp);
    if (shot_wanted && shot_buf) {
        memcpy(shot_buf, px, w * h * 2);
        for (int y = 0; y < h; y++) {
            int x0, lw;
            const uint16_t *l = pv_row(y, &x0, &lw);
            if (l) memcpy(shot_buf + (y * w + x0) * 2, l, lw * 2);
        }
        shot_w = w;
        shot_h = h;
        shot_wanted = false;
        xSemaphoreGive(shot_ready);
    }
    // Portrait is already the panel's orientation: send straight from the
    // frame. Each chunk is byte-swapped into internal RAM while the one before
    // it goes out (2026-11-03: 32 ms a frame through a rotated copy before).
    const lv_color_format_t cf = lv_display_get_color_format(disp);
    const uint8_t *src = px;
    if (lv_display_get_rotation(disp) != LV_DISPLAY_ROTATION_0) {
        lv_draw_sw_rotate(px, rotated, w, h, lv_draw_buf_width_to_stride(w, cf),
                          lv_draw_buf_width_to_stride(PANEL_W, cf), lv_display_get_rotation(disp), cf);
        src = rotated;
    }
    int in_flight = 0, cur = 0;
    for (int y = 0; y < PANEL_H; y += CHUNK_ROWS, src += CHUNK_BYTES, cur ^= 1) {
        if (in_flight == 2) {   // the older one (this buffer) must be out first
            xSemaphoreTake(chunk_done, portMAX_DELAY);
            in_flight--;
        }
        const uint32_t *in = (const uint32_t *)src;
        uint32_t *out = (uint32_t *)chunk_bufs[cur];
        for (int i = 0; i < CHUNK_BYTES / 4; i++) {
            const uint32_t v = in[i];
            out[i] = (v & 0x00ff00ffu) << 8 | (v >> 8 & 0x00ff00ffu);   // two pixels, each byte-swapped
        }
        for (int r = 0; r < CHUNK_ROWS; r++) {   // the palette list over it, at its scroll position
            int x0, lw;
            const uint32_t *l = (const uint32_t *)pv_row(y + r, &x0, &lw);
            if (!l) continue;
            uint32_t *o = (uint32_t *)(chunk_bufs[cur] + r * PANEL_W + x0);
            for (int i = 0; i < lw / 2; i++) o[i] = (l[i] & 0x00ff00ffu) << 8 | (l[i] >> 8 & 0x00ff00ffu);
        }
        esp_lcd_panel_draw_bitmap(panel, 0, y, PANEL_W, y + CHUNK_ROWS, chunk_bufs[cur]);
        in_flight++;
    }
    while (in_flight--) xSemaphoreTake(chunk_done, portMAX_DELAY);
    flush_count++;
    flush_us += esp_timer_get_time() - t0;
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
    chunk_done = xSemaphoreCreateCounting(2, 0);
    for (int i = 0; i < 2; i++) chunk_bufs[i] = heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (DISPLAY_ROTATION != LV_DISPLAY_ROTATION_0) rotated = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(chunk_done && chunk_bufs[0] && chunk_bufs[1] && (rotated || DISPLAY_ROTATION == LV_DISPLAY_ROTATION_0),
                        ESP_ERR_NO_MEM, TAG, "no memory");

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


// Preset grid buttons: user data is the button number (1..10). A short tap
// sets preset N; holding bumps preset N + 10 until release.
static lv_obj_t *preset_held;

static void on_preset_event(lv_event_t *ev)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(ev);
    lv_obj_t *btn = lv_event_get_target_obj(ev);
    const lv_event_code_t code = lv_event_get_code(ev);
    if (!action_cb) return;
    if (code == LV_EVENT_SHORT_CLICKED) {
        action_cb(DISPLAY_TAP_PRESET, true, n);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        preset_held = btn;
        action_cb(DISPLAY_HOLD_PRESET, true, n + DISPLAY_PRESET_HOLD_OFFSET);
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && preset_held == btn) {
        preset_held = NULL;
        action_cb(DISPLAY_HOLD_PRESET, false, n + DISPLAY_PRESET_HOLD_OFFSET);
    }
}

static int64_t slider_hold_until;  // don't let telemetry move a control just touched
static lv_obj_t *auto_switch, *react_select;  // preset page controls, follow the eyes' telemetry
static lv_obj_t *fins_switch, *fins_label, *fins_detail;  // follow WLED's fin state

static void on_control_event(lv_event_t *ev)
{
    lv_obj_t *obj = lv_event_get_target_obj(ev);
    if (!action_cb) return;
    slider_hold_until = esp_timer_get_time() + 1500000;
    if (obj == auto_switch) {
        action_cb(DISPLAY_SET_AUTO, true, lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0);
    } else if (obj == fins_switch) {
        action_cb(DISPLAY_SET_FINS, true, lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0);
    } else if (obj == react_select) {
        const uint32_t sel = lv_buttonmatrix_get_selected_button(obj);
        if (sel <= NL_REACT_PEAKS) action_cb(DISPLAY_SET_REACT, true, (int)sel);
    }
}

static void on_event(lv_event_t *ev)
{
    const display_action_t a = (display_action_t)(intptr_t)lv_event_get_user_data(ev);
    const lv_event_code_t code = lv_event_get_code(ev);
    if (!action_cb) return;
    if (a == DISPLAY_SLIDE_EYES || a == DISPLAY_SLIDE_WLED) {
        if (code == LV_EVENT_RELEASED) {
            action_cb(a, false, lv_slider_get_value(lv_event_get_target_obj(ev)));
            slider_hold_until = esp_timer_get_time() + 1500000;
        }
        return;
    }
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_PRESSED) action_cb(a, true, 0);
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) action_cb(a, false, 0);
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
static lv_obj_t *screen, *pages, *pagem, *pagek, *page0, *page1, *pages_survey, *pad_flash, *pad_black;  // main, Live and Standby, details, presets, survey
static lv_obj_t *event_labels[3];  // one per page
#define event_label event_labels[0]
static volatile int page_wanted = -1;  // page change asked for over serial

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

// with_bar: a brightness slider along the bottom of the card.
// Portrait (DISPLAY_ROTATION 0 or 180): the cards stack down the 172-pixel-wide
// screen. Landscape (90 or 270): they sit side by side.
#define PORTRAIT (DISPLAY_ROTATION == LV_DISPLAY_ROTATION_0 || DISPLAY_ROTATION == LV_DISPLAY_ROTATION_180)

static void make_card(card_t *c, lv_obj_t *parent, const char *title, uint32_t accent, bool with_bar, int w, int h)
{
    c->card = lv_obj_create(parent);
    lv_obj_remove_flag(c->card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(c->card, w, h);
    lv_obj_set_style_bg_color(c->card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_width(c->card, 0, 0);
    lv_obj_set_style_radius(c->card, 10, 0);
    lv_obj_set_style_pad_all(c->card, 8, 0);
    lv_obj_set_flex_flow(c->card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c->card, 2, 0);

    c->title = label(c->card, &lv_font_montserrat_14, accent);
    lv_label_set_text(c->title, title);
    // Preset names: no scrolling. Portrait wraps onto a second line, landscape
    // cuts the name off with "...".
    c->big = label(c->card, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_set_width(c->big, LV_PCT(100));
    lv_label_set_long_mode(c->big, PORTRAIT ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
    c->line1 = label(c->card, &lv_font_montserrat_16, COL_TEXT);
    c->line2 = label(c->card, &lv_font_montserrat_14, COL_TITLE);
    if (PORTRAIT) {  // narrow cards: let the detail lines wrap
        lv_obj_set_width(c->line1, LV_PCT(100));
        lv_obj_set_width(c->line2, LV_PCT(100));
        lv_label_set_long_mode(c->line1, LV_LABEL_LONG_WRAP);
        lv_label_set_long_mode(c->line2, LV_LABEL_LONG_WRAP);
    }
    c->bar = NULL;
    if (with_bar) {
        c->bar = lv_slider_create(c->card);
        lv_obj_set_size(c->bar, LV_PCT(92), 8);
        lv_obj_set_style_margin_top(c->bar, 8, 0);
        lv_obj_set_style_margin_left(c->bar, 6, 0);
        lv_slider_set_range(c->bar, 0, 255);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(COL_DIM), LV_PART_MAIN);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(accent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(c->bar, lv_color_hex(COL_TEXT), LV_PART_KNOB);
        lv_obj_set_style_pad_all(c->bar, 3, LV_PART_KNOB);
        lv_obj_set_ext_click_area(c->bar, 14);  // easier to grab
    }
}

static lv_obj_t *make_pad(const char *text, uint32_t color, display_action_t action, int x_ofs)
{
    lv_obj_t *b = lv_button_create(page0);
    if (PORTRAIT) {  // two big pads side by side along the bottom
        lv_obj_set_size(b, 80, 70);
        lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, x_ofs == -8 ? -4 : -88, -4);
        lv_obj_set_style_pad_hor(b, 2, 0);
    } else {
        lv_obj_set_size(b, 96, 26);
        lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, x_ofs, -3);
    }
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

// Page 2: square preset buttons, 2 columns by 5 rows in portrait (5 by 2 in
// landscape), numbered 1-10, styled like the cards.
static void build_preset_page(void)
{
    const int cols = PORTRAIT ? 2 : 5, rows = PORTRAIT ? 5 : 2;
    const int w = lv_display_get_horizontal_resolution(NULL), h = lv_display_get_vertical_resolution(NULL);
    const int gap = 6, top = PORTRAIT ? 124 : 40, bottom = PORTRAIT ? 48 : 22;
    int side = (w - 8 - gap * (cols - 1)) / cols;
    const int by_height = (h - top - bottom - gap * (rows - 1)) / rows;
    if (by_height < side) side = by_height;

    lv_obj_t *title = label(page1, &lv_font_montserrat_14, COL_WLED);
    lv_label_set_text(title, "PRESETS");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, PORTRAIT ? 10 : 8);

    // AUTO: the eyes change presets by themselves (off once a preset is picked).
    auto_switch = lv_switch_create(page1);
    lv_obj_set_size(auto_switch, 44, 24);
    lv_obj_align(auto_switch, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_set_style_bg_color(auto_switch, lv_color_hex(COL_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_color(auto_switch, lv_color_hex(COL_EYES), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(auto_switch, on_control_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *auto_label = label(page1, &lv_font_montserrat_14, COL_TITLE);
    lv_label_set_text(auto_label, "AUTO");
    lv_obj_align_to(auto_label, auto_switch, LV_ALIGN_OUT_LEFT_MID, -6, 0);

    // How the eyes react to sound: each preset's choice, or all on beats / peaks.
    static const char *react_map[] = { "Preset", "Beats", "Peaks", "" };
    react_select = lv_buttonmatrix_create(page1);
    lv_buttonmatrix_set_map(react_select, react_map);
    lv_buttonmatrix_set_button_ctrl_all(react_select, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(react_select, true);
    lv_buttonmatrix_set_button_ctrl(react_select, NL_REACT_PRESET, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(react_select, PORTRAIT ? w - 8 : 260, 34);
    if (PORTRAIT) lv_obj_align(react_select, LV_ALIGN_TOP_MID, 0, 36);
    else lv_obj_align(react_select, LV_ALIGN_TOP_RIGHT, -60, 2);
    lv_obj_set_style_bg_color(react_select, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(react_select, 0, 0);
    lv_obj_set_style_pad_all(react_select, 0, 0);
    lv_obj_set_style_pad_column(react_select, 4, 0);
    lv_obj_set_style_bg_color(react_select, lv_color_hex(COL_CARD), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(react_select, lv_color_hex(COL_EYES), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(react_select, lv_color_hex(COL_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(react_select, lv_color_hex(COL_BG), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(react_select, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(react_select, 8, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(react_select, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(react_select, on_control_event, LV_EVENT_VALUE_CHANGED, NULL);

    // FINS: WLED's fin LEDs follow the fins' angle (off: they stay at the
    // modelled angle). The label says what WLED is using.
    fins_switch = lv_switch_create(page1);
    lv_obj_set_size(fins_switch, 44, 24);
    lv_obj_set_style_bg_color(fins_switch, lv_color_hex(COL_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_color(fins_switch, lv_color_hex(COL_WLED), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(fins_switch, on_control_event, LV_EVENT_VALUE_CHANGED, NULL);
    fins_label = label(page1, &lv_font_montserrat_14, COL_TITLE);
    lv_label_set_text(fins_label, "FINS");
    fins_detail = label(page1, &lv_font_montserrat_14, COL_DIM);
    lv_label_set_text(fins_detail, "");
    if (PORTRAIT) {  // a row of its own under the reaction selector, the angles under that
        lv_obj_align(fins_switch, LV_ALIGN_TOP_RIGHT, -6, 78);
        lv_obj_align(fins_label, LV_ALIGN_TOP_LEFT, 8, 82);
        lv_obj_set_width(fins_detail, w - 16);
        lv_label_set_long_mode(fins_detail, LV_LABEL_LONG_DOT);
        lv_obj_align(fins_detail, LV_ALIGN_TOP_LEFT, 8, 104);
    } else {         // next to the page title
        lv_obj_align_to(fins_label, title, LV_ALIGN_OUT_RIGHT_MID, 16, 0);
        lv_obj_align_to(fins_switch, fins_label, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
        lv_obj_align_to(fins_detail, fins_switch, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    }

    lv_obj_t *grid = lv_obj_create(page1);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, gap, 0);
    lv_obj_set_style_pad_column(grid, gap, 0);
    lv_obj_set_size(grid, cols * side + (cols - 1) * gap, rows * side + (rows - 1) * gap);
    const int grid_h = rows * side + (rows - 1) * gap;
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, top + (h - top - bottom - grid_h) / 2);  // centred in the space
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);

    for (int n = 1; n <= 10; n++) {
        lv_obj_t *b = lv_obj_create(grid);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(b, side, side);
        lv_obj_set_style_bg_color(b, lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x232733), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_t *num = label(b, &lv_font_montserrat_28, COL_TEXT);
        lv_label_set_text_fmt(num, "%d", n);
        lv_obj_align(num, LV_ALIGN_CENTER, 0, side >= 60 ? -6 : 0);
        if (side >= 60) {
            lv_obj_t *hint = label(b, &lv_font_montserrat_14, COL_DIM);
            lv_label_set_text_fmt(hint, "hold %d", n + DISPLAY_PRESET_HOLD_OFFSET);
            lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
        }
        lv_obj_add_event_cb(b, on_preset_event, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)n);
        lv_obj_add_event_cb(b, on_preset_event, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)n);
        lv_obj_add_event_cb(b, on_preset_event, LV_EVENT_RELEASED, (void *)(intptr_t)n);
        lv_obj_add_event_cb(b, on_preset_event, LV_EVENT_PRESS_LOST, (void *)(intptr_t)n);
    }

    event_labels[1] = label(page1, &lv_font_montserrat_16, COL_TITLE);
    lv_obj_set_width(event_labels[1], w - 16);
    lv_label_set_long_mode(event_labels[1], PORTRAIT ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
    lv_obj_align(event_labels[1], LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_label_set_text(event_labels[1], "tap: preset  hold: bump");
}

static void set_event_text(const char *text, uint32_t color)
{
    for (int i = 0; i < 3; i++) {
        if (!event_labels[i]) continue;
        lv_obj_set_style_text_color(event_labels[i], lv_color_hex(color), 0);
        lv_label_set_text(event_labels[i], text);
    }
}

// ------------------------------------------------------------------ main page
// Portrait, top to bottom: the power status bar (where the power comes from,
// a phone-style battery icon with the charge, then small lines: charging or
// time left, the pack, the lights, temperatures or a problem), LIVE (what
// the shark shows), STANDBY (what TAKE will show; rows that differ are
// bright), then the keypad: target, page, lock, links and the event line.
#define COL_LIVE     0xff3030
#define COL_STANDBY  0x30e060
#define COL_FINS     0xff8000

typedef struct {
    lv_obj_t *panel, *title, *note, *val[5], *key[5];
} scene_panel_t;

static struct {
    lv_obj_t *power, *pct, *in_pct, *in_usb, *flow_in, *flow_out, *in_va, *out_wa, *problem, *no_board;
    lv_obj_t *t_cell[2], *t_slash, *t_die[2], *pack_v;   // row 3; [1]: the copy 1 px right that makes bold
    bool flow_pulsing;
    scene_panel_t live, stby;
    lv_obj_t *target, *target_name, *page, *latch, *lock, *links, *keys, *log;
} m;

static lv_obj_t *make_panel(lv_obj_t *parent, int y, int h, uint32_t border)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(p, 4, y);
    lv_obj_set_size(p, 164, h);
    lv_obj_set_style_bg_color(p, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_radius(p, 10, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_border_color(p, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(p, border ? 2 : 0, 0);
    return p;
}

static lv_obj_t *at(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w)
{
    lv_obj_t *l = label(parent, font, color);
    lv_obj_set_pos(l, x, y);
    if (w) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    return l;
}

// ------------------------------------------------------------------ the sharks
// Side views of the shark from the brains' preview streams: Live under the power
// bar, Standby under the eyes, each with its fins laid flat beneath it. They go by
// the sender's role, so a TAKE swaps the pictures and the top one is always what's
// on the LEDs. Unlit LEDs stay faintly visible, and so does the whole shark with no
// stream, so its shape is always there.
#define SHARK_W        164
#define SHARK_H        82
#define SHARK_DIM      0x18C3   // RGB565, the same grey as FIN_UNLIT
#define SHARK_LOST_MS  2000
#define SHARK_LIVE     0
#define SHARK_STANDBY  1
#define SHARK_VIEWS    2

static struct {
    lv_obj_t *canvas, *fin_canvas;
    uint16_t *px;
    uint32_t *fin_px;              // ARGB8888
    uint8_t leds[LED_POS_COUNT];
    volatile bool dirty;
    volatile int64_t rx_us;
    bool blank;                    // showing the no-stream shark
    volatile uint32_t rx_frames;
    uint32_t drawn;
} shark[SHARK_VIEWS] = { { .dirty = true }, { .dirty = true } };

void display_preview(bool standby, const void *payload, size_t len)
{
    if (len <= NL_PREVIEW_HEADER_LEN) return;
    const nl_preview_t *m = payload;
    size_t n = len - NL_PREVIEW_HEADER_LEN;
    if (m->start >= LED_POS_COUNT) return;
    if (m->start + n > LED_POS_COUNT) n = LED_POS_COUNT - m->start;
    typeof(&shark[0]) v = &shark[standby ? SHARK_STANDBY : SHARK_LIVE];
    taskENTER_CRITICAL(&lock);
    memcpy(v->leds + m->start, m->rgb332, n);
    taskEXIT_CRITICAL(&lock);
    v->rx_us = esp_timer_get_time();
    if (m->flags & NL_PREVIEW_LAST) {
        v->dirty = true;
        v->rx_frames++;
    }
}

static void fins_paint(int view, const uint8_t *leds);

static void shark_draw_view(int view)
{
    typeof(&shark[0]) v = &shark[view];
    if (!v->canvas) return;
    const bool stream = v->rx_us && esp_timer_get_time() - v->rx_us < SHARK_LOST_MS * 1000LL;
    if (stream ? !v->dirty : v->blank) return;
    static uint8_t now[LED_POS_COUNT];
    taskENTER_CRITICAL(&lock);
    memcpy(now, v->leds, sizeof(now));
    v->dirty = false;
    taskEXIT_CRITICAL(&lock);
    memset(v->px, 0, SHARK_W * SHARK_H * 2);
    for (int i = 0; i < LED_POS_COUNT; i++) {
        const uint8_t c = stream ? now[i] : 0;
        const uint16_t col = !c ? SHARK_DIM : ((c >> 5) * 31 / 7) << 11 | (((c >> 2) & 7) * 63 / 7) << 5 | (c & 3) * 31 / 3;
        const int x = shark_pos[i][0], y = shark_pos[i][1];
        for (int dy = 0; dy < 2 && y + dy < SHARK_H; dy++)
            for (int dx = 0; dx < 2 && x + dx < SHARK_W; dx++)
                if (col != SHARK_DIM || v->px[(y + dy) * SHARK_W + x + dx] == 0) v->px[(y + dy) * SHARK_W + x + dx] = col;
    }
    v->blank = !stream;
    v->drawn++;
    lv_obj_invalidate(v->canvas);
    fins_paint(view, stream ? now : NULL);
}

static void shark_create(int view, lv_obj_t *page, int y)
{
    shark[view].px = heap_caps_malloc(SHARK_W * SHARK_H * 2, MALLOC_CAP_SPIRAM);
    if (!shark[view].px) return;
    memset(shark[view].px, 0, SHARK_W * SHARK_H * 2);
    shark[view].canvas = lv_canvas_create(page);
    lv_canvas_set_buffer(shark[view].canvas, shark[view].px, SHARK_W, SHARK_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(shark[view].canvas, 4, y);
}

// ------------------------------------------------------------------ the fins
// Under each shark: the fins laid flat, seen from above. Under Live, a padlock between
// them showing whether they're locked at the modelled angle or follow their
// angle sensors. The whole band, screen edge to screen edge, is a hidden touch
// key for KEY_FIN_SENSOR; a card appears behind the fins only while that key is down.
#define FINS_Y         154
#define FINS_BAND_H    84
#define SHARK2_Y       326      // under the eyes
#define FIN_UNLIT      0xFF181B1Du   // ARGB8888
#define COL_FIN_CARD   0x2A2E33
#define COL_FIN_BORDER 0x6A7076
#define COL_LOCKED     0xFFA000
#define COL_SHACKLE    0xA0A6AC

static struct {
    lv_obj_t *card, *lock_body, *lock_shackle, *lock_leg;
    int8_t shown;          // the padlock's state now (fin_sensor; -2: not drawn yet)
    bool lit;
} fin = { .shown = -2 };
static volatile bool fin_key_down;

void display_fin_key(bool down) { fin_key_down = down; }

// One frame of the fin LEDs (RGB332), or all unlit with no stream, through the
// fins' shape: each pixel takes its nearest LED (fin_pos.h).
static void fins_paint(int view, const uint8_t *leds)
{
    if (!shark[view].fin_canvas) return;
    uint32_t *px = shark[view].fin_px;
    uint32_t cols[256];
    if (leds) for (int i = 0; i < 256; i++) cols[i] = i ? 0xFF000000u | ((i >> 5) * 255 / 7) << 16 | (((i >> 2) & 7) * 255 / 7) << 8 | (i & 3) * 255 / 3 : FIN_UNLIT;
    for (int i = 0; i < FIN_PIXELS; i++) px[fin_mask[i].at] = leds ? cols[leds[FIN_FIRST_LED + fin_mask[i].led]] : FIN_UNLIT;
    lv_obj_invalidate(shark[view].fin_canvas);
}

static void fins_canvas(int view, lv_obj_t *parent, int y)
{
    shark[view].fin_px = heap_caps_calloc(FIN_W * FIN_H, 4, MALLOC_CAP_SPIRAM);
    if (!shark[view].fin_px) return;
    shark[view].fin_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(shark[view].fin_canvas, shark[view].fin_px, FIN_W, FIN_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(shark[view].fin_canvas, (172 - FIN_W) / 2, y);
    lv_obj_remove_flag(shark[view].fin_canvas, LV_OBJ_FLAG_CLICKABLE);
    fins_paint(view, NULL);
}

// Locked: the grey shackle closed into the amber body. Unlocked: the shackle
// lifted straight up, its right end clear of the body and its left leg still
// in it, the whole padlock faded.
static void fins_lock_show(int state)
{
    if (state == fin.shown) return;
    fin.shown = (int8_t)state;
    const bool open = state > 0;
    const lv_opa_t opa = open ? 64 : LV_OPA_COVER;   // about 25%
    lv_obj_set_style_bg_color(fin.lock_body, lv_color_hex(state < 0 ? COL_DIM : COL_LOCKED), 0);
    lv_obj_set_pos(fin.lock_shackle, 80, open ? 13 : 19);
    lv_obj_set_height(fin.lock_shackle, open ? 14 : 20);
    if (open) lv_obj_remove_flag(fin.lock_leg, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(fin.lock_leg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(fin.lock_body, opa, 0);
    lv_obj_set_style_opa(fin.lock_shackle, opa, 0);
    lv_obj_set_style_opa(fin.lock_leg, opa, 0);
}

static void fins_build(void);

// Straight from the key, not the 250 ms status update, so it tracks the press.
static void fins_key_show(void)
{
    if (!fin.card) return;
    if (fin_key_down != fin.lit) {
        fin.lit = fin_key_down;
        if (fin.lit) lv_obj_remove_flag(fin.card, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(fin.card, LV_OBJ_FLAG_HIDDEN);
    }
    taskENTER_CRITICAL(&lock);
    const int state = status.fin_sensor;
    taskEXIT_CRITICAL(&lock);
    fins_lock_show(state);
}

// ------------------------------------------------------------------ charge graph
// The bottom of the main page: the pack's charge over the last hour, one
// pixel-wide column per slice of time, newest on the right, 1 px per percent.
// Kept in RAM only, so it starts empty after a restart; no reading: no column.
#define GRAPH_W        DISPLAY_GRAPH_COLUMNS
#define GRAPH_H        100
#define GRAPH_SLICE_MS (3600000 / GRAPH_W)
#define GRAPH_OK       0x07E0   // RGB565: green
#define GRAPH_LOW      0xFD20   // orange
#define GRAPH_CRITICAL 0xF800   // red

static lv_obj_t *graph_canvas;
static uint16_t *graph_px;
static int8_t graph_pct[GRAPH_W];
static int64_t graph_next_us;

// ------------------------------------------------------------------ the eyes
// Stills of what the eyes show (eye_thumb.c), starboard on the left, port on the right,
// redrawn only when the preset, a look or the eyes' state changes.
#define EYE_Y   242
#define EYE_GAP 6
static lv_obj_t *eye_canvas[2];
static uint16_t *eye_px[2];
static nl_eye_look_t eye_look_in;
static volatile bool eye_look_ready;
static volatile int eye_test_preset = -1, eye_test_state;
static uint32_t eye_look_gen;

void display_eye_look(const nl_eye_look_t *look)
{
    taskENTER_CRITICAL(&lock);
    eye_look_in = *look;
    eye_look_ready = true;
    taskEXIT_CRITICAL(&lock);
}

void display_eye_test(int preset, int state)
{
    eye_test_state = state;
    eye_test_preset = preset;
}

static void eyes_create(lv_obj_t *page)
{
    const int x[2] = { (172 - 2 * EYE_THUMB_SIZE - EYE_GAP) / 2, (172 + EYE_GAP) / 2 };
    for (int i = 0; i < 2; i++) {
        eye_px[i] = heap_caps_malloc(EYE_THUMB_SIZE * EYE_THUMB_SIZE * 2, MALLOC_CAP_SPIRAM);
        if (!eye_px[i]) return;
        eye_thumb_unlit(eye_px[i], EYE_THUMB_SIZE, SHARK_DIM);
        eye_canvas[i] = lv_canvas_create(page);
        lv_canvas_set_buffer(eye_canvas[i], eye_px[i], EYE_THUMB_SIZE, EYE_THUMB_SIZE, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(eye_canvas[i], x[i], EYE_Y);
    }
}

static void eyes_update(const nl_eye_telemetry_t *e, bool fresh)
{
    if (!eye_canvas[1]) return;
    if (eye_look_ready) {
        static nl_eye_look_t look;
        taskENTER_CRITICAL(&lock);
        look = eye_look_in;
        eye_look_ready = false;
        taskEXIT_CRITICAL(&lock);
        if (preset_set_look(&look, -1) >= 0) eye_look_gen++;
    }
    const int test = eye_test_preset;
    const int want = test >= 0 ? test : fresh ? e->preset : -1;
    const int state = test >= 0 ? eye_test_state : e->state;
    // Past the built-in presets is a look: the newest we've heard, if any.
    const int index = want < preset_count ? want : preset_look_index();
    static int shown_index = -2, shown_state;
    static uint32_t shown_gen;
    if (index == shown_index && state == shown_state && (index < preset_count || eye_look_gen == shown_gen)) return;
    shown_index = index;
    shown_state = state;
    shown_gen = eye_look_gen;
    const int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < 2; i++) {
        if (index < 0) eye_thumb_unlit(eye_px[i], EYE_THUMB_SIZE, SHARK_DIM);
        else eye_thumb_draw(eye_px[i], EYE_THUMB_SIZE, preset_get(index), eye_thumb_lid(state), i == 1);   // port: spirals mirrored
        lv_obj_invalidate(eye_canvas[i]);
    }
    ESP_LOGI(TAG, "eyes: preset %d state %d drawn in %d ms", index, state, (int)((esp_timer_get_time() - t0) / 1000));
}

static int8_t graph_restored[GRAPH_W];
static volatile bool graph_restore_ready;

void display_graph_restore(const int8_t pct[DISPLAY_GRAPH_COLUMNS])
{
    memcpy(graph_restored, pct, GRAPH_W);
    graph_restore_ready = true;
}

static void graph_update(int pct)
{
    if (!graph_canvas) return;
    const int64_t now = esp_timer_get_time();
    const bool restore = graph_restore_ready;
    if (now < graph_next_us && !restore) return;
    if (restore) {
        graph_restore_ready = false;
        for (int x = 0; x < GRAPH_W; x++) if (graph_pct[x] < 0) graph_pct[x] = graph_restored[x];
    }
    if (now >= graph_next_us) {
        graph_next_us = now + GRAPH_SLICE_MS * 1000LL;
        memmove(graph_pct, graph_pct + 1, GRAPH_W - 1);
        graph_pct[GRAPH_W - 1] = (int8_t)(pct > 100 ? 100 : pct);
    }
    memset(graph_px, 0, GRAPH_W * GRAPH_H * 2);
    for (int x = 0; x < GRAPH_W; x++) {
        const int v = graph_pct[x];
        if (v <= 0) continue;
        const pack_level_t level = pack_level(v);
        const uint16_t col = level == PACK_CRITICAL ? GRAPH_CRITICAL : level == PACK_LOW ? GRAPH_LOW : GRAPH_OK;
        for (int y = GRAPH_H - v; y < GRAPH_H; y++) graph_px[y * GRAPH_W + x] = col;
    }
    lv_obj_invalidate(graph_canvas);
}

static void graph_build(void)
{
    graph_px = heap_caps_calloc(GRAPH_W * GRAPH_H, 2, MALLOC_CAP_SPIRAM);
    if (!graph_px) return;
    memset(graph_pct, -1, sizeof(graph_pct));
    graph_canvas = lv_canvas_create(pagem);
    lv_canvas_set_buffer(graph_canvas, graph_px, GRAPH_W, GRAPH_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(graph_canvas, 0, 640 - GRAPH_H);
    lv_obj_set_style_opa(graph_canvas, 64, 0);   // about 25%
}

// The STANDBY panel and the target box work as keys by touch too (TAKE, and the next target).
static void on_key_touch(lv_event_t *ev)
{
    const ctl_key_t key = (ctl_key_t)(intptr_t)lv_event_get_user_data(ev);
    const lv_event_code_t code = lv_event_get_code(ev);
    if (!action_cb) return;
    if (code == LV_EVENT_PRESSED) action_cb(DISPLAY_KEY, true, key);
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) action_cb(DISPLAY_KEY, false, key);
}

static void touch_key(lv_obj_t *obj, ctl_key_t key)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, on_key_touch, LV_EVENT_PRESSED, (void *)(intptr_t)key);
    lv_obj_add_event_cb(obj, on_key_touch, LV_EVENT_RELEASED, (void *)(intptr_t)key);
    lv_obj_add_event_cb(obj, on_key_touch, LV_EVENT_PRESS_LOST, (void *)(intptr_t)key);
}

// LIVE: body, eyes, fins, brightness and the body's look (palette and layer);
// STANDBY: body, eyes, fins, the look, then what TAKE does.
static void scene_panel(scene_panel_t *sp, int y, const char *title, uint32_t color, bool live)
{
    static const char *live_keys[] = { "BODY", "EYES", "FINS", "BRI", "LOOK" }, *stby_keys[] = { "BODY", "EYES", "FINS", "LOOK", "" };
    static const uint32_t live_cols[] = { COL_WLED, COL_EYES, COL_FINS, COL_TITLE, COL_WLED }, stby_cols[] = { COL_WLED, COL_EYES, COL_FINS, COL_WLED, COL_TITLE };
    const char *const *keys = live ? live_keys : stby_keys;
    const uint32_t *key_cols = live ? live_cols : stby_cols;
    const int rows = 5;
    sp->panel = make_panel(pagek, y, 157, color);
    sp->title = at(sp->panel, &lv_font_montserrat_16, color, 8, 5, 0);
    lv_label_set_text(sp->title, title);
    sp->note = at(sp->panel, &lv_font_montserrat_14, COL_TITLE, 82, 7, 76);
    lv_obj_set_style_text_align(sp->note, LV_TEXT_ALIGN_RIGHT, 0);
    for (int i = 0; i < rows; i++) {
        sp->key[i] = at(sp->panel, &lv_font_montserrat_14, key_cols[i], 8, 30 + i * 25, 0);
        lv_label_set_text(sp->key[i], keys[i]);
        sp->val[i] = at(sp->panel, &lv_font_montserrat_16, COL_TEXT, 54, 28 + i * 25, 104);
    }
}

static void fins_build(void)
{
    lv_obj_t *band = lv_obj_create(pagem);   // the hidden touch area
    lv_obj_remove_style_all(band);
    lv_obj_set_pos(band, 0, FINS_Y);
    lv_obj_set_size(band, 172, FINS_BAND_H);
    lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    touch_key(band, KEY_FIN_SENSOR);

    fin.card = lv_obj_create(band);
    lv_obj_remove_flag(fin.card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(fin.card, 4, 0);
    lv_obj_set_size(fin.card, 164, FINS_BAND_H);
    lv_obj_set_style_bg_color(fin.card, lv_color_hex(COL_FIN_CARD), 0);
    lv_obj_set_style_radius(fin.card, 10, 0);
    lv_obj_set_style_pad_all(fin.card, 0, 0);
    lv_obj_set_style_border_color(fin.card, lv_color_hex(COL_FIN_BORDER), 0);
    lv_obj_set_style_border_width(fin.card, 2, 0);
    lv_obj_add_flag(fin.card, LV_OBJ_FLAG_HIDDEN);

    fins_canvas(SHARK_LIVE, band, (FINS_BAND_H - FIN_H) / 2);

    fin.lock_shackle = lv_obj_create(band);   // behind the body: a rounded loop
    lv_obj_remove_style_all(fin.lock_shackle);
    lv_obj_remove_flag(fin.lock_shackle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(fin.lock_shackle, 12);
    lv_obj_set_style_radius(fin.lock_shackle, 6, 0);
    lv_obj_set_style_border_width(fin.lock_shackle, 3, 0);
    lv_obj_set_style_border_side(fin.lock_shackle, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(fin.lock_shackle, lv_color_hex(COL_SHACKLE), 0);
    lv_obj_set_style_border_opa(fin.lock_shackle, LV_OPA_COVER, 0);

    fin.lock_leg = lv_obj_create(band);       // unlocked: the left leg down to the body
    lv_obj_remove_style_all(fin.lock_leg);
    lv_obj_remove_flag(fin.lock_leg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(fin.lock_leg, 80, 27);
    lv_obj_set_size(fin.lock_leg, 3, 4);
    lv_obj_set_style_bg_color(fin.lock_leg, lv_color_hex(COL_SHACKLE), 0);
    lv_obj_set_style_bg_opa(fin.lock_leg, LV_OPA_COVER, 0);

    fin.lock_body = lv_obj_create(band);
    lv_obj_remove_style_all(fin.lock_body);
    lv_obj_remove_flag(fin.lock_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(fin.lock_body, 77, 31);   // the closed padlock centred on the fins' roots
    lv_obj_set_size(fin.lock_body, 18, 15);
    lv_obj_set_style_radius(fin.lock_body, 3, 0);
    lv_obj_set_style_bg_opa(fin.lock_body, LV_OPA_COVER, 0);
    fins_lock_show(-1);
}

// An iOS-style battery: a thin translucent outline with round corners and a
// small cap, the fill inset inside it: white, green while charging (with a
// bolt), red at 20 % or less.
#define BATT_W 27
#define BATT_H 13
#define COL_IOS_GREEN 0x34c759
#define COL_IOS_RED   0xff3b30
#define COL_IOS_BLUE  0x0a84ff
#define FLOW_W        172       // the flow line under the batteries

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    return o;
}

// Montserrat 12 with tabular figures, for readings that change: every digit
// as wide as the widest, centred in it, and U+2007 (figure space, "\xe2\x80\x87")
// a blank of that width for padding, so numbers don't shift as they update.
// The glyphs are Montserrat's own (the copy shares its data).
#define FIG_SPACE "\xe2\x80\x87"
static lv_font_t font_tab12;
static uint16_t tab_digit_w;

static bool tab_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t next)
{
    const bool digit = letter >= '0' && letter <= '9', fig = letter == 0x2007;
    (void)next;   // no kerning: it would shift a character by the digit after it
    if (!lv_font_montserrat_12.get_glyph_dsc(font, dsc, fig ? ' ' : letter, 0)) return false;
    if (digit || fig) {
        dsc->ofs_x += (tab_digit_w - dsc->adv_w) / 2;
        dsc->adv_w = tab_digit_w;
    }
    return true;
}

static void tab_font_init(void)
{
    font_tab12 = lv_font_montserrat_12;
    font_tab12.get_glyph_dsc = tab_glyph_dsc;
    lv_font_glyph_dsc_t g;
    for (uint32_t c = '0'; c <= '9'; c++)
        if (lv_font_montserrat_12.get_glyph_dsc(&lv_font_montserrat_12, &g, c, 0) && g.adv_w > tab_digit_w) tab_digit_w = g.adv_w;
}

typedef struct {
    lv_obj_t *outline, *nub, *fill, *bolt;
} battery_t;

static battery_t batt_pack, batt_in;   // the internal pack (right), the LEMO input's battery (left)

static void build_battery(battery_t *b, lv_obj_t *parent, int x, int y)
{
    b->outline = box(parent, x, y, BATT_W, BATT_H, 4);
    lv_obj_set_style_bg_opa(b->outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b->outline, 1, 0);
    lv_obj_set_style_border_color(b->outline, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_border_opa(b->outline, LV_OPA_40, 0);
    b->nub = box(parent, x + BATT_W + 1, y + 4, 2, BATT_H - 8, 1);
    lv_obj_set_style_bg_color(b->nub, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_bg_opa(b->nub, LV_OPA_40, 0);
    b->fill = box(b->outline, 1, 1, 0, BATT_H - 4, 2);   // positions start inside the 1 px border: 2 px from the edge
    b->bolt = label(b->outline, &lv_font_montserrat_12, COL_TEXT);
    lv_label_set_text(b->bolt, LV_SYMBOL_CHARGE);
    lv_obj_center(b->bolt);
}

// pct < 0: no reading (an empty, dimmer outline).
static void set_battery(battery_t *b, int pct, bool charging)
{
    lv_obj_set_style_border_opa(b->outline, pct < 0 ? LV_OPA_20 : LV_OPA_40, 0);
    lv_obj_set_style_bg_opa(b->nub, pct < 0 ? LV_OPA_20 : LV_OPA_40, 0);
    lv_obj_set_width(b->fill, pct > 0 ? 1 + (BATT_W - 5) * pct / 100 : 0);
    lv_obj_set_style_bg_color(b->fill, lv_color_hex(charging ? COL_IOS_GREEN : pct <= 20 ? COL_IOS_RED : COL_TEXT), 0);
    if (charging) lv_obj_remove_flag(b->bolt, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(b->bolt, LV_OBJ_FLAG_HIDDEN);
}

static void show_battery(battery_t *b, bool on)
{
    lv_obj_t *parts[] = { b->outline, b->nub };
    for (int i = 0; i < 2; i++) {
        if (on) lv_obj_remove_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void flow_pulse_cb(void *obj, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

// The flow line: green (to the pack) for split_permille of it, blue (to the
// lights) for the rest; starved: all grey and pulsing; nothing in: dim grey.
static void set_flow(int split_permille, bool starved)
{
    const bool pulse = starved && split_permille >= 0;
    if (pulse != m.flow_pulsing) {
        m.flow_pulsing = pulse;
        lv_anim_delete(m.flow_in, flow_pulse_cb);
        lv_obj_set_style_bg_opa(m.flow_in, LV_OPA_COVER, 0);
        if (pulse) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, m.flow_in);
            lv_anim_set_exec_cb(&a, flow_pulse_cb);
            lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
            lv_anim_set_duration(&a, 600);
            lv_anim_set_playback_duration(&a, 600);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
        }
    }
    if (split_permille < 0 || pulse) {   // one grey line
        lv_obj_set_width(m.flow_in, FLOW_W);
        lv_obj_set_style_bg_color(m.flow_in, lv_color_hex(pulse ? COL_TITLE : COL_DIM), 0);
        lv_obj_add_flag(m.flow_out, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const int w = FLOW_W * split_permille / 1000;
    lv_obj_set_width(m.flow_in, w);
    lv_obj_set_style_bg_color(m.flow_in, lv_color_hex(COL_IOS_GREEN), 0);
    lv_obj_remove_flag(m.flow_out, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(m.flow_out, w);
    lv_obj_set_width(m.flow_out, FLOW_W - w);
    lv_obj_set_style_bg_color(m.flow_out, lv_color_hex(COL_IOS_BLUE), 0);
}

// One temperature in row 3: grey, orange near its limit, bold red past it.
static void set_temp(lv_obj_t *l[2], int c10, temp_level_t level)
{
    char text[24];
    if (c10 == INT16_MIN) snprintf(text, sizeof(text), "--\xc2\xb0");
    else {
        const int c = (c10 + (c10 < 0 ? -5 : 5)) / 10;
        snprintf(text, sizeof(text), "%s%d\xc2\xb0", c >= 0 && c < 10 ? FIG_SPACE : "", c);
    }
    const uint32_t col = level == TEMP_BAD ? COL_IOS_RED : level == TEMP_WARN ? COL_WARN : COL_TITLE;
    for (int i = 0; i < 2; i++) {
        lv_label_set_text(l[i], text);
        lv_obj_set_style_text_color(l[i], lv_color_hex(col), 0);
    }
    if (level == TEMP_BAD) lv_obj_remove_flag(l[1], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(l[1], LV_OBJ_FLAG_HIDDEN);
}

static void build_main_page(void)
{
    // The power status bar: the LEMO input left (an external battery, or a
    // USB-C supply), the internal pack right; the flow line (the LEMO's current:
    // green to the pack, blue to the lights); the LEMO's volts and amps left,
    // the lights' watts and amps right; small lines below.
    m.power = make_panel(pagem, 4, 62, 0);    // edge to edge, no side padding
    lv_obj_set_x(m.power, 0);
    lv_obj_set_width(m.power, 172);
    lv_obj_set_style_radius(m.power, 0, 0);
    lv_obj_set_style_bg_color(m.power, lv_color_hex(0x000000), 0);
    tab_font_init();   // every number in the bar: Montserrat 12 with fixed-width digits
    build_battery(&batt_in, m.power, 0, 7);
    m.in_pct = at(m.power, &font_tab12, COL_TEXT, 34, 7, 44);   // the same small font as the lines below
    m.in_usb = at(m.power, &font_tab12, COL_TEXT, 0, 7, 78);
    m.pct = at(m.power, &font_tab12, COL_TEXT, 100, 7, 38);
    lv_obj_set_style_text_align(m.pct, LV_TEXT_ALIGN_RIGHT, 0);
    build_battery(&batt_pack, m.power, 142, 7);
    m.flow_in = box(m.power, 0, 24, 0, 2, 0);
    m.flow_out = box(m.power, 0, 24, FLOW_W, 2, 0);
    m.in_va = at(m.power, &font_tab12, COL_TEXT, 0, 29, 84);
    m.out_wa = at(m.power, &font_tab12, COL_TEXT, 102, 29, 70);
    lv_obj_set_style_text_align(m.out_wa, LV_TEXT_ALIGN_RIGHT, 0);
    // row 3: the cells' and the charger's temperatures left, the pack's voltage right
    for (int i = 0; i < 2; i++) {
        m.t_cell[i] = at(m.power, &font_tab12, COL_TITLE, i, 44, 0);
        m.t_die[i] = at(m.power, &font_tab12, COL_TITLE, 28 + i, 44, 0);
    }
    m.t_slash = at(m.power, &font_tab12, COL_TITLE, 22, 44, 0);
    lv_label_set_text(m.t_slash, "/");
    m.pack_v = at(m.power, &font_tab12, COL_TITLE, 112, 44, 60);
    lv_obj_set_style_text_align(m.pack_v, LV_TEXT_ALIGN_RIGHT, 0);
    m.problem = at(m.power, &font_tab12, COL_BAD, 0, 44, 172);   // in place of row 3 when there is one
    m.no_board = at(m.power, &font_tab12, COL_TEXT, 0, 7, 172);  // no power board: top row, centred
    lv_obj_set_style_text_align(m.no_board, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(m.no_board, LV_OPA_20, 0);         // the colour of the dim battery outlines
    lv_label_set_text(m.no_board, "???");

    eyes_create(pagem);
    shark_create(SHARK_LIVE, pagem, 70);
    fins_build();
    shark_create(SHARK_STANDBY, pagem, SHARK2_Y);
    fins_canvas(SHARK_STANDBY, pagem, SHARK2_Y + SHARK_H + (FINS_BAND_H - FIN_H) / 2);
    graph_build();

    // The next page: Live, Standby and the keypad's state.
    scene_panel(&m.live, 4, "LIVE", COL_LIVE, true);
    scene_panel(&m.stby, 167, "STANDBY", COL_STANDBY, false);
    touch_key(m.stby.panel, KEY_TAKE);

    m.target = make_panel(pagek, 330, 50, 0);
    m.target_name = at(m.target, &lv_font_montserrat_28, COL_BG, 8, 10, 0);
    m.page = at(m.target, &lv_font_montserrat_14, COL_BG, 100, 6, 56);
    m.latch = at(m.target, &lv_font_montserrat_14, COL_BG, 100, 26, 56);
    lv_obj_set_style_text_align(m.page, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(m.latch, LV_TEXT_ALIGN_RIGHT, 0);
    touch_key(m.target, KEY_NONE);   // the callback turns KEY_NONE into "next target"

    m.lock = at(pagek, &lv_font_montserrat_16, COL_BAD, 8, 386, 156);
    m.links = at(pagek, &lv_font_montserrat_14, COL_TITLE, 8, 408, 156);
    m.keys = at(pagek, &lv_font_montserrat_14, COL_TITLE, 8, 426, 156);
    m.log = at(pagek, &lv_font_montserrat_14, COL_TITLE, 8, 444, 156);
    event_labels[2] = label(pagek, &lv_font_montserrat_14, COL_TITLE);
    lv_obj_set_width(event_labels[2], 156);
    lv_label_set_long_mode(event_labels[2], LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(event_labels[2], 8, 466);
    lv_label_set_text(event_labels[2], "Nibbles base");
}

static void fmt_preset(char *out, size_t n, int preset, const char *name)
{
    if (preset <= 0) snprintf(out, n, "--");
    else if (name && name[0]) snprintf(out, n, "%d %s", preset, name);
    else snprintf(out, n, "Preset %d", preset);
}

static void fill_scene(scene_panel_t *sp, const ctl_scene_t *sc, const char *body, const char *eyes, const char *fins, int bri,
                       uint8_t bright_rows)
{
    char text[40];
    fmt_preset(text, sizeof(text), sc->body, body);
    lv_label_set_text(sp->val[0], text);
    fmt_preset(text, sizeof(text), sc->eyes, eyes);
    lv_label_set_text(sp->val[1], text);
    if (sc->fins > 0) fmt_preset(text, sizeof(text), sc->fins, fins);   // a fin preset
    lv_label_set_text(sp->val[2], sc->fins < 0 ? "--" : sc->fins ? text : "body");
    if (bri >= 0) lv_label_set_text_fmt(sp->val[3], "%d%%", (bri * 100 + 127) / 255);
    for (int i = 0; i < 3; i++)
        lv_obj_set_style_text_color(sp->val[i], lv_color_hex(bright_rows & (1 << (i + 1)) ? COL_TEXT : COL_DIM), 0);
}

// LOOK: the body's palette ("--" where not known, or a new preset's own) and the layer over it.
static void set_look(lv_obj_t *val, const ctl_layers_t *l, const char *pal_name)
{
    char pal[28] = "--";
    if (l->body_pal >= 0) {
        if (pal_name[0]) snprintf(pal, sizeof(pal), "%s", pal_name);
        else snprintf(pal, sizeof(pal), "pal %d", l->body_pal);
    }
    if (l->body_layer > 0) lv_label_set_text_fmt(val, "%s + L%d", pal, l->body_layer - NL_LAYER_PRESET_BASE);
    else lv_label_set_text(val, pal);
}

static uint32_t target_col(int t)
{
    static const uint32_t cols[] = { 0xffffff, COL_WLED, COL_EYES, COL_FINS };
    return t >= 0 && t <= 3 ? cols[t] : COL_DIM;
}

static void fmt_ma(char *out, size_t n, int ma)
{
    const int a = ma < 0 ? -ma : ma;
    snprintf(out, n, "%s%d.%d A", ma < 0 ? "-" : "", a / 1000, a % 1000 / 100);
}

static void main_update(const display_status_t *s)
{
    const power_status_t *p = &s->power;
    char text[48], a[16];
    // POWER: the LEMO input (left) and the internal pack (right)
    const bool board = p->have_charger || p->have_monitor || p->have_expander;
    const chg_state_t cs = (chg_state_t)p->chg_state;
    const bool charging = p->docked && p->have_charger && p->charger_ok && cs != CHG_DONE && cs != CHG_NOT_CHARGING;
    set_battery(&batt_pack, p->pack_pct, charging);
    graph_update(p->pack_mv ? p->pack_pct : -1);
    eyes_update(&s->eyes, s->eyes_fresh);
    if (p->pack_pct >= 0) lv_label_set_text_fmt(m.pct, "%d%%", p->pack_pct);
    else lv_label_set_text(m.pct, "");   // no reading: just the dim battery
    lv_obj_set_style_text_color(m.pct, lv_color_hex(p->pack_pct < 0 ? COL_DIM : COL_TEXT), 0);
    if (p->input == INPUT_SUPPLY) {   // a USB-C charger: its voltage, no charge to show
        show_battery(&batt_in, false);
        lv_label_set_text(m.in_pct, "");
        lv_label_set_text_fmt(m.in_usb, LV_SYMBOL_USB " %d.%d V", p->vbus_mv / 1000, p->vbus_mv % 1000 / 100);
    } else {
        show_battery(&batt_in, true);
        set_battery(&batt_in, p->input_pct, false);
        lv_label_set_text(m.in_usb, "");
        if (p->input_pct >= 0) lv_label_set_text_fmt(m.in_pct, "%d%%", p->input_pct);
        else lv_label_set_text(m.in_pct, "");   // nothing plugged in: just the dim battery
        lv_obj_set_style_text_color(m.in_pct, lv_color_hex(p->input_pct < 0 ? COL_DIM : COL_TEXT), 0);
    }
    // The flow line; row 2: the LEMO's volts and amps, or with nothing plugged
    // in the time left on the pack (left), and the lights' amps and watts (right)
    set_flow(p->have_charger ? p->in_split : -1, p->starved);
    if (p->have_charger && p->input != INPUT_NONE) {
        const int v10 = (p->vbus_mv + 50) / 100, a10 = (p->in_ma + 50) / 100;   // tenths
        lv_label_set_text_fmt(m.in_va, "%s%d.%dV %d.%dA", v10 < 100 ? FIG_SPACE : "", v10 / 10, v10 % 10, a10 / 10, a10 % 10);
        lv_obj_set_style_text_color(m.in_va, lv_color_hex(COL_TEXT), 0);
    } else if (p->runtime_min >= 0) {
        const int mins = p->runtime_min < 99 * 60 + 59 ? p->runtime_min : 99 * 60 + 59;   // hhHmmM, at most 99H59M
        lv_label_set_text_fmt(m.in_va, "%02dH%02dM", mins / 60, mins % 60);
        lv_obj_set_style_text_color(m.in_va, lv_color_hex(p->runtime_min > 45 ? COL_TEXT : COL_WARN), 0);
    } else {
        lv_label_set_text(m.in_va, "--H--M");
        lv_obj_set_style_text_color(m.in_va, lv_color_hex(COL_DIM), 0);
    }
    if (p->pole_off) {
        lv_label_set_text(m.out_wa, "POLE OFF");
        lv_obj_set_style_text_color(m.out_wa, lv_color_hex(COL_WARN), 0);
    } else if (p->have_monitor) {
        const int ma = p->light_ma > 0 ? p->light_ma : 0, a10 = (ma + 50) / 100, w = (p->light_mw + 500) / 1000;
        // amps: one digit before the point (100 W at the light bus's 12 V or more is under 10 A)
        lv_label_set_text_fmt(m.out_wa, "%d.%dA %dW", a10 / 10, a10 % 10, w);
        lv_obj_set_style_text_color(m.out_wa, lv_color_hex(COL_TEXT), 0);
    } else {
        lv_label_set_text(m.out_wa, "-.-A --W");
        lv_obj_set_style_text_color(m.out_wa, lv_color_hex(COL_DIM), 0);
    }
    // row 3: the temperatures (cells / charger) and the pack's voltage
    if (p->have_charger) {
        set_temp(m.t_cell, p->cell_c10, cell_temp_level(p->cell_c10));
        set_temp(m.t_die, p->die_c10, die_temp_level(p->die_c10));
    } else {
        set_temp(m.t_cell, INT16_MIN, TEMP_OK);
        set_temp(m.t_die, INT16_MIN, TEMP_OK);
    }
    if (p->pack_pct >= 0) lv_label_set_text_fmt(m.pack_v, "%s%d.%02dV", p->pack_mv < 10000 ? FIG_SPACE : "", p->pack_mv / 1000, p->pack_mv % 1000 / 10);
    else lv_label_set_text(m.pack_v, "--.--V");
    // A problem, worst first, takes row 3's place in red, with a red border.
    const char *problem = NULL;
    const bool no_board = !board;   // quiet: "???", no border
    if (no_board) lv_obj_remove_flag(m.no_board, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(m.no_board, LV_OBJ_FLAG_HIDDEN);
    if (p->have_charger && !p->charger_ok) problem = "Charger NOT set up";
    else if (p->pole_off) problem = NULL;   // asked for; row 2 shows POLE OFF
    else if (p->lights_off) problem = "LIGHTS SWITCHED OFF";
    else if (p->have_expander && !p->efuse_pg) problem = "eFuse tripped / off";
    else if (p->fault0 || p->fault1) {
        snprintf(text, sizeof(text), "Charger fault %02x %02x", p->fault0, p->fault1);
        problem = text;
    } else if (p->ts_flags & 0x09) problem = (p->ts_flags & 1) ? "Cells too hot to charge" : "Cells too cold to charge";
    lv_obj_t *row3[] = { m.t_cell[0], m.t_cell[1], m.t_slash, m.t_die[0], m.t_die[1], m.pack_v };
    for (int i = 0; i < 6; i++) {
        if (problem) lv_obj_add_flag(row3[i], LV_OBJ_FLAG_HIDDEN);
        else if (row3[i] != m.t_cell[1] && row3[i] != m.t_die[1]) lv_obj_remove_flag(row3[i], LV_OBJ_FLAG_HIDDEN);   // bold copies: set_temp
    }
    if (problem) {
        lv_label_set_text(m.problem, problem);
        lv_obj_remove_flag(m.problem, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(m.problem, LV_OBJ_FLAG_HIDDEN);
    // Drawn inside the card, so a problem doesn't push the contents off the screen edge.
    lv_obj_set_style_outline_width(m.power, problem ? 2 : 0, 0);
    lv_obj_set_style_outline_pad(m.power, -2, 0);
    lv_obj_set_style_outline_color(m.power, lv_color_hex(COL_BAD), 0);

    // LIVE and STANDBY
    fill_scene(&m.live, &s->live, s->live_body, s->live_eyes, s->live_fins, s->wled_fresh ? s->brightness : -1, 0xff);
    lv_label_set_text(m.live.note, s->wled_fresh || s->eyes_fresh ? "" : "not heard");
    set_look(m.live.val[4], &s->layers, s->live_pal_name);
    fill_scene(&m.stby, &s->standby, s->stby_body, s->stby_eyes, s->stby_fins, -1, s->diff);
    set_look(m.stby.val[3], &s->stby_layers, s->stby_pal_name);
    lv_obj_set_style_text_color(m.stby.val[3], lv_color_hex(s->stby_layers.body_pal != s->layers.body_pal ||
                                                             s->stby_layers.body_layer != s->layers.body_layer ? COL_TEXT : COL_DIM), 0);
    lv_label_set_text(m.stby.val[4], s->diff ? "hold TAKE" : "same as Live");
    lv_obj_set_style_text_color(m.stby.val[4], lv_color_hex(s->diff ? COL_STANDBY : COL_DIM), 0);
    lv_label_set_text(m.stby.note, s->take_preview ? "PREVIEW" : "");
    lv_obj_set_style_text_color(m.stby.note, lv_color_hex(COL_STANDBY), 0);
    lv_obj_set_style_bg_color(m.stby.panel, lv_color_hex(s->take_preview ? 0x0c3a18 : COL_CARD), 0);

    // Keypad
    lv_obj_set_style_bg_color(m.target, lv_color_hex(s->keys_locked ? COL_DIM : target_col(s->target)), 0);
    lv_label_set_text(m.target_name, ctl_target_name((ctl_target_t)s->target));
    lv_label_set_text(m.page, ctl_page_name(s->page));
    lv_label_set_text(m.latch, s->latch ? "LATCH" : "");
    lv_label_set_text(m.lock, s->keys_locked ? "LOCKED: hold UNLOCK" : "");
    lv_label_set_text_fmt(m.links, "Eyes %s  WLED %s  ch %d", s->eyes_fresh ? "ok" : "--", s->wled_fresh ? "ok" : "--", s->channel);
    lv_obj_set_style_text_color(m.links, lv_color_hex(s->eyes_fresh && s->wled_fresh ? COL_TITLE : COL_WARN), 0);
    if (s->keypads_found) lv_label_set_text_fmt(m.keys, "Keypads %d of %d", s->keypads_found, s->keypads_total);
    else lv_label_set_text(m.keys, "No keypads found");
    lv_label_set_text(m.log, s->log_text);
    lv_label_set_text(m.live.title, s->ab_live < 0 ? "LIVE" : s->ab_live ? "LIVE L" : "LIVE R");   // the Right Brain (0) or the Left (1)
    lv_label_set_text(m.stby.title, s->ab_live < 0 ? "STANDBY" : s->ab_live ? "STANDBY R" : "STANDBY L");
}


// ------------------------------------------------------------------ the palette viewer
// A full screen over the pages: a title with the place in the list, then
// CTL_PALETTE_ROWS rows, each the key's number (fixed), the palette's name and
// its colours as WLED blends its 16 (a CRGBPalette16, wled_palettes.h). The
// names and colours are drawn one row beyond each end in a list that slides:
// a move of one row (UP/DOWN) scrolls it into place; a screenful jumps.
#define PV_TOP     44
#define PV_ROW     ((640 - PV_TOP) / CTL_PALETTE_ROWS)   // 74 for 8 rows, 37 for 16
#define PV_LIST    (CTL_PALETTE_ROWS + 2)                 // rows drawn: one above and one below the screen
#define PV_LIST_X  28                                     // the list's left edge: the numbers stay left of it
#define PV_LIST_W  (172 - PV_LIST_X)
#define PV_BAR_X   2                                      // within the list
#define PV_BAR_W   (PV_LIST_W - PV_BAR_X - 4)
#define PV_BAR_Y   18                                     // within the row, under the name
#define PV_BAR_H   (PV_ROW - PV_BAR_Y - 4)
#define PV_ANIM_MS 150                                    // a one-row scroll; quicker when steps come faster

static portMUX_TYPE pv_lock = portMUX_INITIALIZER_UNLOCKED;
static struct { bool on; int first; uint32_t held; } pv_want;
static struct {
    lv_obj_t *root, *range, *target, *canvas;   // canvas: hidden, only to draw the names into px
    lv_obj_t *num[CTL_PALETTE_ROWS];
    uint16_t *px;
    bool on;
    int first;                // drawn; -1 none yet
    int64_t last_step_us;
    uint32_t held;
    int16_t live, stby;       // the target's palettes as last marked
    uint8_t tgt;
} pv = { .first = -1, .live = -2, .stby = -2, .tgt = 0xff };

void display_palettes(bool on, int first, uint32_t held)
{
    taskENTER_CRITICAL(&pv_lock);
    pv_want.on = on;
    pv_want.first = first;
    pv_want.held = held;
    taskEXIT_CRITICAL(&pv_lock);
}

static uint16_t rgb565(int r, int g, int b) { return (uint16_t)((r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3); }
static int pv_wrap(int i) { return (i % WLED_PALETTE_COUNT + WLED_PALETTE_COUNT) % WLED_PALETTE_COUNT; }

static void pv_build(void)
{
    pv.px = heap_caps_calloc(PV_LIST_W * PV_LIST * PV_ROW, 2, MALLOC_CAP_SPIRAM);
    if (!pv.px) return;
    pv.root = lv_obj_create(screen);
    lv_obj_remove_style_all(pv.root);
    lv_obj_set_size(pv.root, 172, 640);
    lv_obj_set_pos(pv.root, 0, 0);
    lv_obj_set_style_bg_color(pv.root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(pv.root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(pv.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = at(pv.root, &lv_font_montserrat_16, COL_TEXT, 6, 3, 100);
    lv_label_set_text(title, "PALETTES");
    pv.target = at(pv.root, &lv_font_montserrat_14, COL_TEXT, 100, 4, 66);
    lv_obj_set_style_text_align(pv.target, LV_TEXT_ALIGN_RIGHT, 0);
    pv.range = at(pv.root, &lv_font_montserrat_12, COL_TITLE, 6, 24, 160);
    // The list isn't an LVGL object: flush_cb copies it into each frame at the
    // scroll position (pv_y), so a scroll costs no drawing. The canvas only
    // gives the names somewhere to be drawn into px.
    pv.canvas = lv_canvas_create(pv.root);
    lv_obj_add_flag(pv.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_buffer(pv.canvas, pv.px, PV_LIST_W, PV_LIST * PV_ROW, LV_COLOR_FORMAT_RGB565);
    for (int i = 0; i < CTL_PALETTE_ROWS; i++) {
        pv.num[i] = at(pv.root, &lv_font_montserrat_16, COL_TEXT, 2, PV_TOP + i * PV_ROW + PV_BAR_Y + (PV_BAR_H - 20) / 2, 24);
        lv_obj_set_style_text_align(pv.num[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_radius(pv.num[i], 4, 0);
        lv_obj_set_style_bg_opa(pv.num[i], LV_OPA_TRANSP, 0);
        lv_label_set_text_fmt(pv.num[i], "%d", i + 1);
    }
    lv_obj_add_flag(pv.root, LV_OBJ_FLAG_HIDDEN);
}

static void pv_frame(void);

// The list for a top row: list row i is palette first - 1 + i, round. The
// names are drawn into the canvas too, so a scroll moves one picture (2026-11-03:
// labels redrawn every frame held it to ~15 frames a second).
static void pv_draw(int first)
{
    memset(pv.px, 0, PV_LIST_W * PV_LIST * PV_ROW * 2);
    lv_layer_t layer;
    lv_canvas_init_layer(pv.canvas, &layer);
    for (int i = 0; i < PV_LIST; i++) {
        const wled_palette_t *p = &wled_palettes[pv_wrap(first - 1 + i)];
        char name[48];
        snprintf(name, sizeof(name), "%s%s", p->name, p->kind == WLED_PAL_DEFAULT ? " (effect's own)" : "");
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.font = &lv_font_montserrat_12;
        d.color = lv_color_hex(p->kind == WLED_PAL_DYNAMIC ? COL_TITLE : COL_TEXT);
        d.text = name;
        d.text_local = 1;
        const lv_area_t a = { PV_BAR_X, i * PV_ROW + 2, PV_BAR_X + PV_BAR_W - 1, i * PV_ROW + PV_BAR_Y - 1 };
        lv_draw_label(&layer, &d, &a);
        uint16_t *row = pv.px + (i * PV_ROW + PV_BAR_Y) * PV_LIST_W;
        for (int x = 0; x < PV_BAR_W; x++) {
            uint16_t c;
            if (p->kind == WLED_PAL_DYNAMIC) {
                c = ((x / 6) & 1) ? 0x2104 : 0x4208;   // stripes: made from the effect's colours, nothing fixed
            } else {
                // WLED's linear blend of its 16 colours, end to end without wrapping
                const int idx = x * 240 / (PV_BAR_W - 1), hi = idx >> 4, lo = idx & 15;
                const uint8_t *a = p->rgb[hi], *b = p->rgb[hi < 15 ? hi + 1 : 15];
                c = rgb565((a[0] * (16 - lo) + b[0] * lo) / 16, (a[1] * (16 - lo) + b[1] * lo) / 16,
                           (a[2] * (16 - lo) + b[2] * lo) / 16);
            }
            for (int y = 0; y < PV_BAR_H; y++) row[y * PV_LIST_W + PV_BAR_X + x] = c;
        }
    }
    lv_canvas_finish_layer(pv.canvas, &layer);
    pv_frame();
    const int last = pv_wrap(first + CTL_PALETTE_ROWS - 1);
    lv_label_set_text_fmt(pv.range, "%d-%d of %d   " LV_SYMBOL_UP " " LV_SYMBOL_DOWN, first + 1, last + 1, WLED_PALETTE_COUNT);
}

// Where the list is: its row 0 at PV_TOP + pv_y (-PV_ROW at rest; a one-row
// scroll slides from 0 or -2 * PV_ROW to that). Display task only.
static int pv_y = -PV_ROW;
static volatile bool pv_compose;   // flush_cb puts the list in the frame

// Asks LVGL for a frame (the list changed or moved): one pixel under the list.
static void pv_frame(void)
{
    const lv_area_t a = { PV_LIST_X, PV_TOP, PV_LIST_X, PV_TOP };
    lv_obj_invalidate_area(pv.root, &a);
}

static void pv_anim_y(void *var, int32_t y)
{
    (void)var;
    pv_y = y;
    pv_frame();
}

// For flush_cb: the list's pixels for screen row y from x = PV_LIST_X, or
// NULL where the list isn't.
static const uint16_t *pv_row(int y, int *x0, int *w)
{
    const int r = y - PV_TOP - pv_y;
    if (!pv_compose || y < PV_TOP || y >= PV_TOP + CTL_PALETTE_ROWS * PV_ROW || r < 0 || r >= PV_LIST * PV_ROW) return NULL;
    *x0 = PV_LIST_X;
    *w = PV_LIST_W;
    return pv.px + r * PV_LIST_W;
}

// Which numbers are held, and the target's Live and Standby palettes.
static void pv_mark(const display_status_t *s, uint32_t held)
{
    const bool fins = s->target == TGT_FINS, eyes = s->target == TGT_EYES;
    const int16_t live = eyes ? -1 : fins ? s->layers.fins_pal : s->layers.body_pal;
    const int16_t stby = eyes ? -1 : fins ? s->stby_layers.fins_pal : s->stby_layers.body_pal;
    if (live == pv.live && stby == pv.stby && pv.held == held && s->target == pv.tgt) return;
    pv.live = live;
    pv.stby = stby;
    pv.tgt = s->target;
    pv.held = held;
    lv_label_set_text(pv.target, eyes ? "EYES: none" : ctl_target_name((ctl_target_t)s->target));
    for (int i = 0; i < CTL_PALETTE_ROWS; i++) {
        const int id = pv_wrap(pv.first + i);
        const bool l = id == live, st = id == stby, h = held >> i & 1;
        const uint32_t bg = h ? 0xffffff : l && st ? 0xffa000 : l ? COL_LIVE : st ? COL_STANDBY : 0;
        lv_obj_set_style_bg_opa(pv.num[i], bg ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(pv.num[i], lv_color_hex(bg), 0);
        lv_obj_set_style_text_color(pv.num[i], lv_color_hex(bg ? COL_BG : COL_TEXT), 0);
    }
}

// On the display task, every loop: show, hide, scroll, mark.
static void pv_show(const display_status_t *s)
{
    if (!pv.root) return;
    taskENTER_CRITICAL(&pv_lock);
    const bool on = pv_want.on;
    const int first = pv_want.first;
    const uint32_t held = pv_want.held;
    taskEXIT_CRITICAL(&pv_lock);
    if (on != pv.on) {
        pv.on = on;
        // The pages under it are hidden too: LVGL would draw them under it every frame.
        // A faster refresh while it shows, for smooth scrolling (frames cost little here).
        lv_timer_t *refr = lv_display_get_refr_timer(lv_display_get_default());
        if (on) {
            lv_obj_remove_flag(pv.root, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(pages, LV_OBJ_FLAG_HIDDEN);
            if (refr) lv_timer_set_period(refr, 15);
        } else {
            lv_obj_remove_flag(pages, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(pv.root, LV_OBJ_FLAG_HIDDEN);
            lv_anim_delete(&pv_y, pv_anim_y);
            if (refr) lv_timer_set_period(refr, LV_DEF_REFR_PERIOD);
        }
        pv_compose = on;
        if (on) pv.first = -1;   // straight to where it is, no slide
    }
    if (!on) return;
    if (first != pv.first) {
        const int d = pv.first < 0 ? 0 : pv_wrap(first - pv.first);
        const int step = d == 1 ? 1 : d == WLED_PALETTE_COUNT - 1 ? -1 : 0;
        pv.first = first;
        pv_draw(first);
        lv_anim_delete(&pv_y, pv_anim_y);
        if (step) {
            // Drawn for the new top row; start where the old one was and slide into place.
            const int64_t now = esp_timer_get_time();
            const int gap_ms = (int)((now - pv.last_step_us) / 1000);
            pv.last_step_us = now;
            const int ms = gap_ms < PV_ANIM_MS + 20 ? (gap_ms > 30 ? gap_ms - 20 : 30) : PV_ANIM_MS;
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, &pv_y);
            lv_anim_set_exec_cb(&a, pv_anim_y);
            lv_anim_set_values(&a, step > 0 ? 0 : -2 * PV_ROW, -PV_ROW);
            lv_anim_set_duration(&a, ms);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            pv_y = -PV_ROW;
            pv_frame();
        }
        pv.live = pv.stby = -2;   // mark again
    }
    pv_mark(s, held);
}


// ------------------------------------------------------------------ survey page
// Page 4: a site survey of the 2.4 GHz channels (other networks' traffic and
// access points, ours left out) and a held button that moves the whole
// network to the recommended channel. Portrait only (172 wide).

static const char *radio_state(const display_status_t *s)
{
    return s->anchoring ? "anchoring" : s->leading ? "searching (leading)" : s->group ? "searching (following)"
         : s->locked ? "locked" : "scanning";
}

static uint32_t radio_state_color(const display_status_t *s)
{
    return s->anchoring || s->leading || s->group ? COL_WARN : s->locked ? COL_OK : COL_BAD;
}

#define SV_ROWS NL_CHANNEL_MAX
static lv_obj_t *sv_now, *sv_button, *sv_info, *sv_best, *sv_move, *sv_move_label;
static lv_obj_t *sv_num[SV_ROWS + 1], *sv_bar[SV_ROWS + 1], *sv_text[SV_ROWS + 1];
static nl_survey_t sv_data;
static bool sv_have, sv_busy;
static volatile bool sv_new;
static int64_t sv_at_us;
static bool sv_can_move;   // locked onto a brain: a move has an anchor to go with

void display_survey(const nl_survey_t *results, bool busy)
{
    taskENTER_CRITICAL(&lock);
    if (results) {
        sv_data = *results;
        sv_have = true;
        sv_at_us = esp_timer_get_time();
    }
    sv_busy = busy;
    sv_new = true;
    taskEXIT_CRITICAL(&lock);
}

static void on_survey_event(lv_event_t *ev)
{
    if (!action_cb) return;
    lv_obj_t *obj = lv_event_get_target_obj(ev);
    if (obj == sv_button) action_cb(DISPLAY_SURVEY, true, 0);
    else if (obj == sv_move && sv_have && sv_data.worth_moving && sv_can_move) action_cb(DISPLAY_MOVE_CHANNEL, true, sv_data.best);
}

static lv_obj_t *sv_btn(lv_obj_t *parent, int y, int h, uint32_t color, const char *text, lv_obj_t **lbl)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 164, h);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_BG), LV_STATE_DISABLED);
    lv_obj_set_style_border_color(b, lv_color_hex(COL_DIM), LV_STATE_DISABLED);
    lv_obj_t *l = label(b, &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_width(l, 150);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (lbl) *lbl = l;
    return b;
}

static void build_survey_page(void)
{
    lv_obj_t *p = pages_survey;
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *t = label(p, &lv_font_montserrat_14, COL_TITLE);
    lv_label_set_text(t, "RADIO SURVEY");
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 6, 6);
    sv_now = label(p, &lv_font_montserrat_14, COL_TEXT);
    lv_obj_align(sv_now, LV_ALIGN_TOP_LEFT, 6, 26);

    sv_button = sv_btn(p, 48, 40, COL_EYES, "SURVEY", NULL);
    lv_obj_add_event_cb(sv_button, on_survey_event, LV_EVENT_SHORT_CLICKED, NULL);
    sv_info = label(p, &lv_font_montserrat_12, COL_TITLE);
    lv_obj_set_width(sv_info, 160);
    lv_label_set_long_mode(sv_info, LV_LABEL_LONG_WRAP);
    lv_obj_align(sv_info, LV_ALIGN_TOP_LEFT, 6, 94);
    lv_label_set_text(sv_info, "Off the network ~6 s to listen to every channel.");

    // One row per channel: number, load bar, access points and airtime.
    for (int c = 1; c <= SV_ROWS; c++) {
        const int y = 130 + (c - 1) * 27;
        sv_num[c] = label(p, &lv_font_montserrat_14, COL_TEXT);
        lv_label_set_text_fmt(sv_num[c], "%d", c);
        lv_obj_set_width(sv_num[c], 20);
        lv_obj_set_style_text_align(sv_num[c], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(sv_num[c], LV_ALIGN_TOP_LEFT, 2, y);
        sv_bar[c] = lv_bar_create(p);
        lv_obj_set_size(sv_bar[c], 66, 12);
        lv_obj_align(sv_bar[c], LV_ALIGN_TOP_LEFT, 28, y + 3);
        lv_obj_set_style_bg_color(sv_bar[c], lv_color_hex(COL_CARD), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sv_bar[c], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(sv_bar[c], 3, LV_PART_MAIN);
        lv_obj_set_style_radius(sv_bar[c], 3, LV_PART_INDICATOR);
        lv_bar_set_range(sv_bar[c], 0, 100);
        lv_bar_set_value(sv_bar[c], 0, LV_ANIM_OFF);
        sv_text[c] = label(p, &lv_font_montserrat_12, COL_TITLE);
        lv_obj_align(sv_text[c], LV_ALIGN_TOP_LEFT, 98, y + 1);
        lv_label_set_text(sv_text[c], "-");
    }

    sv_best = label(p, &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_width(sv_best, 160);
    lv_label_set_long_mode(sv_best, LV_LABEL_LONG_WRAP);
    lv_obj_align(sv_best, LV_ALIGN_TOP_LEFT, 6, 130 + SV_ROWS * 27 + 4);
    sv_move = sv_btn(p, 640 - 6 - 64, 64, COL_WARN, "HOLD TO MOVE", &sv_move_label);
    lv_obj_add_event_cb(sv_move, on_survey_event, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_state(sv_move, LV_STATE_DISABLED);
}

static uint32_t sv_load_color(unsigned load)
{
    return load < 100 ? COL_OK : load < 300 ? COL_WARN : COL_BAD;
}

static void survey_show(void)
{
    nl_survey_t d;
    bool have, busy;
    int64_t at;
    taskENTER_CRITICAL(&lock);
    d = sv_data;
    have = sv_have;
    busy = sv_busy;
    at = sv_at_us;
    taskEXIT_CRITICAL(&lock);
    if (busy) {
        lv_label_set_text(sv_info, "Surveying... off the network for a few seconds.");
        lv_obj_add_state(sv_button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(sv_button, LV_STATE_DISABLED);
    }
    if (!have) return;
    if (!busy) {
        unsigned aps = 0;
        for (int c = 1; c <= SV_ROWS; c++) aps += d.ch[c].aps;
        lv_label_set_text_fmt(sv_info, "%u other networks. Bar: the air we'd share there.", aps);
    }
    unsigned top = 200;
    for (int c = 1; c <= SV_ROWS; c++)
        if (d.ch[c].load > top) top = d.ch[c].load;
    for (int c = 1; c <= SV_ROWS; c++) {
        const nl_survey_chan_t *ch = &d.ch[c];
        lv_bar_set_value(sv_bar[c], (int)(ch->load * 100u / top), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(sv_bar[c], lv_color_hex(sv_load_color(ch->load)), LV_PART_INDICATOR);
        lv_label_set_text_fmt(sv_text[c], "%uap %u.%u%%", ch->aps, ch->busy_pml / 10, ch->busy_pml % 10);
        const uint32_t col = c == d.best && d.worth_moving ? COL_OK : c == d.current ? COL_EYES : c > 11 ? COL_DIM : COL_TEXT;
        lv_obj_set_style_text_color(sv_num[c], lv_color_hex(col), 0);
    }
    (void)at;
    if (d.worth_moving)
        lv_label_set_text_fmt(sv_best, "Best: ch %d (%u vs %u here)", d.best, d.ch[d.best].load, d.ch[d.current].load);
    else
        lv_label_set_text_fmt(sv_best, "Ch %d is as good as any: stay", d.current);
    lv_label_set_text_fmt(sv_move_label, d.worth_moving ? "HOLD TO MOVE ALL TO CH %d" : "HOLD TO MOVE", d.best);
}

static void survey_update(const display_status_t *s)
{
    lv_label_set_text_fmt(sv_now, "Ch %d, %s", s->channel, radio_state(s));
    sv_can_move = s->locked && !s->group && !s->leading && !s->anchoring;
    if (sv_new) {
        sv_new = false;
        survey_show();
    }
    const bool ok = sv_have && sv_data.worth_moving && sv_can_move && !sv_busy && sv_data.current == s->channel;
    if (ok) lv_obj_remove_state(sv_move, LV_STATE_DISABLED);
    else lv_obj_add_state(sv_move, LV_STATE_DISABLED);
}

static void ui_build(void)
{
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Four pages side by side: the main page (power and the shark), swipe left
    // for Live and Standby, again for the preset grid, again for the details
    // (the radio cards and bump pads). The last two are kept for reference and
    // debugging while the rest of the UI gets built.
    pages = lv_tileview_create(screen);
    lv_obj_set_style_bg_opa(pages, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(pages, LV_SCROLLBAR_MODE_OFF);
    pagem = lv_tileview_add_tile(pages, 0, 0, LV_DIR_RIGHT);
    pagek = lv_tileview_add_tile(pages, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page1 = lv_tileview_add_tile(pages, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    page0 = lv_tileview_add_tile(pages, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    pages_survey = lv_tileview_add_tile(pages, 4, 0, LV_DIR_LEFT);
    build_main_page();
    build_survey_page();

    lv_obj_t *row = lv_obj_create(page0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    if (PORTRAIT) {
        lv_obj_set_size(row, 172, 520);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_pad_row(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        make_card(&eyes_card, row, "EYES", COL_EYES, true, 164, 172);
        make_card(&wled_card, row, "WLED", COL_WLED, true, 164, 172);
        make_card(&radio_card, row, "RADIO", COL_OK, false, 164, 112);
    } else {
        lv_obj_set_size(row, 640, 140);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        make_card(&eyes_card, row, "EYES", COL_EYES, true, 204, 136);
        make_card(&wled_card, row, "WLED", COL_WLED, true, 204, 136);
        make_card(&radio_card, row, "RADIO", COL_OK, false, 204, 136);
    }

    event_label = label(page0, &lv_font_montserrat_16, COL_TITLE);
    if (PORTRAIT) {  // above the pads, wrapping onto a second line if needed
        lv_obj_set_width(event_label, 160);
        lv_label_set_long_mode(event_label, LV_LABEL_LONG_WRAP);
        lv_obj_align(event_label, LV_ALIGN_BOTTOM_LEFT, 8, -80);
    } else {
        lv_obj_align(event_label, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    }
    lv_label_set_text(event_label, "Nibbles base");

    // Tapping a card steps that side's preset.
    lv_obj_add_flag(eyes_card.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(eyes_card.card, on_event, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)DISPLAY_TAP_EYES);
    lv_obj_add_event_cb(eyes_card.card, on_event, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)DISPLAY_HOLD_EYES);
    lv_obj_add_flag(wled_card.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wled_card.card, on_event, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)DISPLAY_TAP_WLED);
    lv_obj_set_style_bg_color(eyes_card.card, lv_color_hex(0x232733), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(wled_card.card, lv_color_hex(0x232733), LV_STATE_PRESSED);
    lv_obj_add_event_cb(eyes_card.bar, on_event, LV_EVENT_RELEASED, (void *)(intptr_t)DISPLAY_SLIDE_EYES);
    lv_obj_add_event_cb(wled_card.bar, on_event, LV_EVENT_RELEASED, (void *)(intptr_t)DISPLAY_SLIDE_WLED);

    // Bump pads: held while touched.
    pad_black = make_pad("BLACKOUT", 0x5a5f6a, DISPLAY_PAD_BLACKOUT, -8);
    pad_flash = make_pad("FLASH", COL_WARN, DISPLAY_PAD_FLASH, -8 - 104);

    build_preset_page();
    pv_build();   // last: over everything
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

static void set_slider(lv_obj_t *slider, int value, bool enabled)
{
    if (enabled) lv_obj_remove_state(slider, LV_STATE_DISABLED);
    else lv_obj_add_state(slider, LV_STATE_DISABLED);
    if (lv_obj_has_state(slider, LV_STATE_PRESSED) || esp_timer_get_time() < slider_hold_until) return;
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
}

static void ui_update(const display_status_t *s)
{
    main_update(s);
    // Eyes
    card_stale(&eyes_card, !s->eyes_fresh);
    if (s->eyes_fresh) {
        const nl_eye_telemetry_t *e = &s->eyes;
        if (e->name[0]) lv_label_set_text_fmt(eyes_card.big, "%.*s", NL_NAME_LEN, e->name);
        else lv_label_set_text_fmt(eyes_card.big, "Preset %d/%d", e->preset + 1, e->preset_count);
        lv_label_set_text_fmt(eyes_card.line1, "%s  %s  %d%%", eye_state(e->state), e->linked ? "linked" : "NOT linked",
                              e->brightness);
        lv_obj_set_style_text_color(eyes_card.line1, lv_color_hex(e->linked ? COL_TEXT : COL_WARN), 0);
        // LVGL's printf has no floats.
        lv_label_set_text_fmt(eyes_card.line2, "%d/%d %s  %d.%d|%d.%d fps  %d bpm", e->preset + 1, e->preset_count,
                              (e->flags & NL_EYE_PEAKS_NOW) ? ((e->flags & NL_EYE_AUTO_CYCLE) ? "auto peaks" : "held peaks")
                                                            : ((e->flags & NL_EYE_AUTO_CYCLE) ? "auto" : "held"),
                              e->fps_x10[0] / 10, e->fps_x10[0] % 10, e->fps_x10[1] / 10, e->fps_x10[1] % 10,
                              (int)(e->tempo_bpm + 0.5f));
        set_slider(eyes_card.bar, (e->brightness * 255 + 50) / 100, false);   // the eyes stay at 100%: shown, not set
        if (esp_timer_get_time() >= slider_hold_until) {  // the preset page controls follow the eyes
            if (e->flags & NL_EYE_AUTO_CYCLE) lv_obj_add_state(auto_switch, LV_STATE_CHECKED);
            else lv_obj_remove_state(auto_switch, LV_STATE_CHECKED);
            const uint32_t r = NL_EYE_REACT(e->flags);
            if (r <= NL_REACT_PEAKS && !lv_buttonmatrix_has_button_ctrl(react_select, r, LV_BUTTONMATRIX_CTRL_CHECKED))
                lv_buttonmatrix_set_button_ctrl(react_select, r, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    } else {
        lv_label_set_text(eyes_card.big, "--");
        lv_label_set_text(eyes_card.line1, "not heard");
        lv_label_set_text(eyes_card.line2, "");
        set_slider(eyes_card.bar, 0, false);
    }

    // Fins: the switch follows WLED (unless just touched); the label shows the angles in use.
    if (s->fins_fresh) {
        const nl_fin_state_t *f = &s->fins;
        if (esp_timer_get_time() >= slider_hold_until) {
            if (f->follow) lv_obj_add_state(fins_switch, LV_STATE_CHECKED);
            else lv_obj_remove_state(fins_switch, LV_STATE_CHECKED);
        }
        lv_obj_remove_state(fins_switch, LV_STATE_DISABLED);
        if (!f->follow) {
            lv_label_set_text(fins_detail, "locked at the model angle");
        } else {
            char parts[2][24];
            for (int i = 0; i < 2; i++) {
                const char *src = f->source[i] == NL_FIN_SRC_SWEEP ? " sweep" : "";
                if (f->angle_cdeg[i] == NL_FIN_NONE) snprintf(parts[i], sizeof(parts[i]), "%c --", i ? 'S' : 'P');
                else snprintf(parts[i], sizeof(parts[i]), "%c %+d%s", i ? 'S' : 'P', (f->angle_cdeg[i] + (f->angle_cdeg[i] < 0 ? -50 : 50)) / 100, src);
            }
            lv_label_set_text_fmt(fins_detail, "%s  %s", parts[0], parts[1]);
        }
        lv_obj_set_style_text_color(fins_label, lv_color_hex(COL_TITLE), 0);
    } else {
        lv_label_set_text(fins_detail, "WLED not heard");
        lv_obj_set_style_text_color(fins_label, lv_color_hex(COL_DIM), 0);
        lv_obj_add_state(fins_switch, LV_STATE_DISABLED);
    }

    // WLED
    card_stale(&wled_card, !s->wled_fresh);
    if (s->wled_fresh) {
        const nl_wled_telemetry_t *w = &s->wled;
        const nl_wled_stream_t *st = &s->stream;
        // With a main board streaming to the display, telemetry comes from the
        // main board (the display only reports the stream): show its preset.
        if (!w->on) lv_label_set_text(wled_card.big, "OFF");
        else if (w->preset && w->name[0]) lv_label_set_text_fmt(wled_card.big, "%.*s", NL_NAME_LEN, w->name);
        else if (w->preset) lv_label_set_text_fmt(wled_card.big, "Preset %d", w->preset);
        else lv_label_set_text(wled_card.big, "No preset");
        if (s->stream_fresh) {   // the display's report on the stream from the main board
            lv_label_set_text_fmt(wled_card.line1, "#%d  bri %d  streamed", w->preset, w->bri);
            lv_obj_set_style_text_color(wled_card.line1, lv_color_hex(st->stalls ? COL_WARN : COL_TEXT), 0);
            lv_label_set_text_fmt(wled_card.line2, "%d fps  %u stalls/min  max %u ms", w->fps, st->stalls, st->max_gap_ms);
        } else {
            lv_label_set_text_fmt(wled_card.line1, "#%d  bri %d  fx %d  pal %d", w->preset, w->bri, w->fx, w->palette);
            lv_obj_set_style_text_color(wled_card.line1, lv_color_hex(COL_TEXT), 0);
            lv_label_set_text_fmt(wled_card.line2, "%d fps   %d LEDs", w->fps, w->leds);
        }
        set_slider(wled_card.bar, w->bri, true);
    } else {
        lv_label_set_text(wled_card.big, "--");
        lv_label_set_text(wled_card.line1, "not heard");
        lv_label_set_text(wled_card.line2, "");
        set_slider(wled_card.bar, 0, false);
    }

    // Radio
    lv_label_set_text_fmt(radio_card.big, "Ch %d", s->channel);
    lv_label_set_text(radio_card.line1, radio_state(s));
    lv_obj_set_style_text_color(radio_card.line1, lv_color_hex(radio_state_color(s)), 0);
    survey_update(s);
    lv_label_set_text_fmt(radio_card.line2, "rx %lu  tx %lu  fail %lu", (unsigned long)s->rx, (unsigned long)s->tx,
                          (unsigned long)s->tx_fail);

    // A held bump shows on the event line (no outline: a border on the
    // screen shrinks its content area and moves every page by its width).
    if (s->bump_action) {
        char text[32];
        snprintf(text, sizeof(text), "BUMP  %s", bump_name(s->bump_action));
        set_event_text(text, COL_WARN);
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
            const int64_t tu = esp_timer_get_time();
            ui_update(&s);
            ui_us += esp_timer_get_time() - tu;
            if (new_event || (last_bump && !s.bump_action)) {
                set_event_text(ev, COL_TITLE);
            }
            last_bump = s.bump_action;
        }
        {
            display_status_t s;
            taskENTER_CRITICAL(&lock);
            s = status;
            taskEXIT_CRITICAL(&lock);
            pv_show(&s);
        }
        if (page_wanted >= 0) {
            // pages: 0 main, 1 Live and Standby, 2 presets, 3 details, 4 survey (tiles in that order)
            lv_tileview_set_tile_by_index(pages, page_wanted, 0, LV_ANIM_OFF);
            page_wanted = -1;
        }
        if (shot_wanted) lv_obj_invalidate(screen);
        if (!pv.on) {   // under the palette viewer they can't be seen: its scrolling gets the frames
            for (int i = 0; i < SHARK_VIEWS; i++) shark_draw_view(i);
            fins_key_show();
        }
        const int64_t t_lv = esp_timer_get_time();
        const uint32_t wait = lv_timer_handler();
        const int64_t t_end = esp_timer_get_time();
        static int64_t shark_log_us, lv_busy_us, lv_max_us;
        const int64_t lv_us = t_end - t_lv;
        lv_busy_us += t_end - now;
        if (lv_us > lv_max_us) lv_max_us = lv_us;
        if (t_end - shark_log_us > 10000000) {
            ESP_LOGI(TAG, "sharks: Live %lu in, %lu drawn; Standby %lu in, %lu drawn; screen busy %d%%, longest refresh %d ms, "
                          "%lu frames, %d ms each to send, %d ms each to draw+send; ui %d ms/s",
                     (unsigned long)shark[SHARK_LIVE].rx_frames, (unsigned long)shark[SHARK_LIVE].drawn,
                     (unsigned long)shark[SHARK_STANDBY].rx_frames, (unsigned long)shark[SHARK_STANDBY].drawn,
                     shark_log_us ? (int)(lv_busy_us * 100 / (t_end - shark_log_us)) : 0, (int)(lv_max_us / 1000),
                     (unsigned long)flush_count, flush_count ? (int)(flush_us / flush_count / 1000) : 0,
                     flush_count ? (int)(refr_us / flush_count / 1000) : 0, (int)(ui_us / 10000));
            refr_us = ui_us = 0;
            flush_count = 0;
            flush_us = 0;
            shark_log_us = t_end;
            lv_busy_us = lv_max_us = 0;
        }
        // At least 10 ms between rounds, 2 under the palette viewer (its frames are cheap: smooth scrolling).
        const uint32_t min_ms = pv_compose ? 2 : 10;
        vTaskDelay(pdMS_TO_TICKS(wait < min_ms ? min_ms : wait > UPDATE_MS ? UPDATE_MS : wait));
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
    lv_display_set_buffers(disp, buf, NULL, FRAME_BYTES,
                           DISPLAY_ROTATION == LV_DISPLAY_ROTATION_0 ? LV_DISPLAY_RENDER_MODE_DIRECT : LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, on_refr, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(disp, on_refr, LV_EVENT_REFR_READY, NULL);
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

// Base64 in 76-character lines.
static void shot_print_b64(const uint8_t *d, size_t n)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[80];
    size_t k = 0;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
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
}

void display_screenshot(bool rle)
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
    const size_t n = (size_t)shot_w * shot_h * 2;
    if (rle) {
        // Runs of one pixel value (the screen is mostly flat colours): a
        // count (1-256, stored as count - 1) and the pixel, little-endian,
        // 3 bytes a run. Typically a few percent of the raw frame.
        static uint8_t *runs;
        if (!runs) runs = heap_caps_malloc(FRAME_BYTES / 2 * 3, MALLOC_CAP_SPIRAM);
        if (!runs) {
            printf("SHOT failed\n");
            return;
        }
        size_t m = 0;
        for (size_t i = 0; i < n;) {
            const uint8_t lo = shot_buf[i], hi = shot_buf[i + 1];
            size_t len = 1;
            while (len < 256 && i + len * 2 < n && shot_buf[i + len * 2] == lo && shot_buf[i + len * 2 + 1] == hi) len++;
            runs[m++] = (uint8_t)(len - 1);
            runs[m++] = lo;
            runs[m++] = hi;
            i += len * 2;
        }
        printf("SHOT %d %d rle565le\n", shot_w, shot_h);
        shot_print_b64(runs, m);
    } else {
        printf("SHOT %d %d rgb565le\n", shot_w, shot_h);
        shot_print_b64(shot_buf, n);
    }
    printf("SHOT END\n");
}

void display_show_page(int page)
{
    page_wanted = page;
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
