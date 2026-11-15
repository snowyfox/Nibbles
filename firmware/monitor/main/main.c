// Live / Standby monitor (2026-11-14): a Waveshare ESP32-S3-Touch-AMOLED-1.64
// (CO5300, 280 x 456 on QSPI, portrait) in the base box. One shows Live, the
// other Standby: the side view of the shark from that side's brain
// (NL_MSG_PREVIEW) and, under it, what the base's switcher says that side is
// (NL_MSG_SHOW): body, eyes, fins, look. Radio only: no cable to the base.
// Which side: by MAC (MONITOR_STANDBY_MACS), or "side live|standby" over USB
// (saved). "shot" prints the screen as the base's does (tools/base_shot.py).
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nibbles_espnow.h"
#include "nibbles_link.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "led_pos.h"

static const char *TAG = "monitor";

// Panel, board v1.1 (Waveshare's docs: V1.1 moved LCD_CS from GPIO 9 to 46,
// and IMU_INT1 to 9; the version silkscreen is by the pin headers on v1.1,
// at the top on v1): QSPI CS 46, CLK 10, D0-3 11-14, RST 21; the panel's
// columns start 20 in. v1.1 also has LCD_TE on 45, TP_INT 18, IMU_INT2 17.
#define LCD_HOST       SPI2_HOST
#define PIN_CS         46          // v1.1 (v1: 9)
#define PIN_SCLK       10
#define PIN_D0         11
#define PIN_D1         12
#define PIN_D2         13
#define PIN_D3         14
#define PIN_RST        21
#define LCD_W          280         // the panel's own (portrait) size
#define LCD_H          456
#define SCR_W          456         // mounted in landscape (the user, 2026-11-14): LVGL
#define SCR_H          280         // draws 456 x 280 and flush_cb turns each band
#define LCD_X_OFFSET   20
#define BUF_ROWS       38
#define BRIGHTNESS     0xD0

#define CMD(c)         ((0x02u << 24) | ((uint32_t)(c) << 8))

// Boards that are the Standby monitor unless told otherwise (the rest: Live).
#define MONITOR_STANDBY_MACS { { 0x28, 0x84, 0x85, 0x8D, 0x4C, 0x4C } }

#define START_CHANNEL  11
#define STREAM_LOST_MS 2000
#define SHOW_LOST_MS   2000
#define SHARK_W        296
#define SHARK_H        150
#define FRAME_MS       40

#define COL_BG      0x000000
#define COL_TITLE   0x7f8794
#define COL_TEXT    0xe8ecf2
#define COL_DIM     0x4a505a
#define COL_WARN    0xffb020
#define COL_LIVE    0xff3030
#define COL_STANDBY 0x30e060
#define COL_EYES    0x36c8ff
#define COL_WLED    0xff7ad9
#define COL_FINS    0xff8000

static int side;   // NL_SHOW_LIVE or NL_SHOW_STANDBY
static bool flipped_from_settings(void);

static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t sent;
static uint8_t *rot_buf;           // a band turned to the panel's orientation (DMA)

// From the radio task.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t incoming[LED_POS_COUNT];
static volatile int64_t last_rx_us, last_show_us;
static volatile uint32_t rx_frames, rx_lost, frame_gen;
static int last_frame = -1;
static nl_show_t show;
static volatile int one_brain_since_us;   // a Live preview heard without any Standby one

// Screenshots.
static volatile bool shot_wanted, shot_filling;
static uint8_t *shot_buf;
static SemaphoreHandle_t shot_ready;

// ------------------------------------------------------------------ panel

static bool on_sent(esp_lcd_panel_io_handle_t h, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sent, &woken);
    return woken == pdTRUE;
}

static void param(uint8_t c, const void *data, size_t n) { esp_lcd_panel_io_tx_param(io, CMD(c), data, n); }

static esp_lcd_panel_handle_t panel;

// As Waveshare's 1.64 demo (07_FactoryProgram): Espressif's SH8601 driver
// (it sends MADCTL and COLMOD, then this list) on this CO5300. The screens
// stayed black on 2026-11-14 until CS moved to GPIO 46 (board v1.1).
static const sh8601_lcd_init_cmd_t init_cmds[] = {
    {0x11, (uint8_t[]){ 0x00 }, 0, 80},
    {0xC4, (uint8_t[]){ 0x80 }, 1, 0},
    {0x35, (uint8_t[]){ 0x00 }, 1, 0},
    {0x53, (uint8_t[]){ 0x20 }, 1, 1},
    {0x63, (uint8_t[]){ 0xFF }, 1, 1},
    {0x51, (uint8_t[]){ 0x00 }, 1, 1},     // dark until the first frame is out
    {0x29, (uint8_t[]){ 0x00 }, 0, 10},
};

static void lcd_init(void)
{
    const spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(PIN_SCLK, PIN_D0, PIN_D1, PIN_D2, PIN_D3,
                                                              SCR_W * BUF_ROWS * 2 + 16);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t cfg = SH8601_PANEL_IO_QSPI_CONFIG(PIN_CS, on_sent, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &cfg, &io));
    sh8601_vendor_config_t vendor = {
        .init_cmds = init_cmds,
        .init_cmds_size = sizeof(init_cmds) / sizeof(init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &dev, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

// The CO5300 takes windows with even start and odd end columns and rows.
static void round_area(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
    if (a->x2 >= SCR_W) a->x2 = SCR_W - 1;   // both sizes are even, so the turned
    if (a->y2 >= SCR_H) a->y2 = SCR_H - 1;   // window starts even and ends odd too
}

static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px)
{
    const int w = lv_area_get_width(a), h = lv_area_get_height(a);
    if (shot_filling && shot_buf)   // as seen: landscape
        for (int y = 0; y < h; y++) memcpy(shot_buf + ((a->y1 + y) * SCR_W + a->x1) * 2, px + y * w * 2, w * 2);
    lv_draw_sw_rgb565_swap(px, w * h);
    // LVGL draws landscape; the CO5300 can't turn the picture itself.
    const lv_display_rotation_t rot = lv_display_get_rotation(disp);
    lv_draw_sw_rotate(px, rot_buf, w, h, w * 2, h * 2, rot, LV_COLOR_FORMAT_RGB565);
    lv_area_t n = *a;
    lv_display_rotate_area(disp, &n);
    esp_lcd_panel_draw_bitmap(panel, n.x1 + LCD_X_OFFSET, n.y1, n.x2 + LCD_X_OFFSET + 1, n.y2 + 1, rot_buf);
    xSemaphoreTake(sent, portMAX_DELAY);
    if (shot_filling && lv_display_flush_is_last(disp)) {
        shot_filling = false;
        xSemaphoreGive(shot_ready);
    }
    lv_display_flush_ready(disp);
}

// ------------------------------------------------------------------ radio

static void on_radio(const uint8_t mac[6], const nl_radio_hdr_t *hdr, const uint8_t *payload, size_t len)
{
    const int64_t now = esp_timer_get_time();
    if (hdr->type == NL_MSG_SHOW && len == sizeof(nl_show_t)) {
        const nl_show_t *s = (const nl_show_t *)payload;
        if (s->side != side) return;
        taskENTER_CRITICAL(&lock);
        show = *s;
        taskEXIT_CRITICAL(&lock);
        last_show_us = now;
        return;
    }
    if (hdr->type != NL_MSG_PREVIEW || len <= NL_PREVIEW_HEADER_LEN) return;
    // The Live brain sends as a WLED, the Standby one as NL_ROLE_WLED_STANDBY.
    const bool from_standby = hdr->role == NL_ROLE_WLED_STANDBY;
    if (from_standby != (side == NL_SHOW_STANDBY)) return;
    const nl_preview_t *m = (const nl_preview_t *)payload;
    size_t n = len - NL_PREVIEW_HEADER_LEN;
    if (m->start >= LED_POS_COUNT) return;
    if (m->start + n > LED_POS_COUNT) n = LED_POS_COUNT - m->start;
    taskENTER_CRITICAL(&lock);
    memcpy(incoming + m->start, m->rgb332, n);
    taskEXIT_CRITICAL(&lock);
    last_rx_us = now;
    if (m->flags & NL_PREVIEW_LAST) {
        if (last_frame >= 0) rx_lost += (uint8_t)(m->frame - last_frame - 1);
        last_frame = m->frame;
        rx_frames++;
        frame_gen++;
    }
}

// ------------------------------------------------------------------ UI

static struct {
    lv_obj_t *banner, *title, *note, *shark, *shark_note;
    lv_obj_t *body, *eyes, *fins, *look, *bri, *foot, *target;
} u;
static uint16_t *shark_px;

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, bool wrap)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        if (!wrap) lv_obj_set_height(l, lv_font_get_line_height(font));
        lv_label_set_long_mode(l, wrap ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
    }
    lv_label_set_text(l, "");
    return l;
}

// Landscape, 456 x 280: the banner across the top; the shark left, the
// target and look on the right; the body preset big along the bottom, eyes
// and fins under it.
static void ui_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    const uint32_t accent = side == NL_SHOW_STANDBY ? COL_STANDBY : COL_LIVE;
    const int rx = SHARK_W + 8, rw = SCR_W - rx - 6;   // the right column

    u.banner = lv_obj_create(scr);
    lv_obj_remove_flag(u.banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(u.banner, 0, 0);
    lv_obj_set_size(u.banner, SCR_W, 40);
    lv_obj_set_style_radius(u.banner, 0, 0);
    lv_obj_set_style_border_width(u.banner, 0, 0);
    lv_obj_set_style_pad_all(u.banner, 0, 0);
    lv_obj_set_style_bg_color(u.banner, lv_color_hex(accent), 0);
    u.title = label(u.banner, &lv_font_montserrat_28, COL_BG, 10, 6, 0, false);
    u.note = label(u.banner, &lv_font_montserrat_20, COL_BG, 240, 10, SCR_W - 250, false);
    lv_obj_set_style_text_align(u.note, LV_TEXT_ALIGN_RIGHT, 0);

    shark_px = heap_caps_calloc(SHARK_W * SHARK_H, 2, MALLOC_CAP_SPIRAM);
    u.shark = lv_canvas_create(scr);
    lv_canvas_set_buffer(u.shark, shark_px, SHARK_W, SHARK_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(u.shark, 4, 42);
    u.shark_note = label(scr, &lv_font_montserrat_14, COL_DIM, 4, 44, SHARK_W, false);   // above the shark's back
    lv_obj_set_style_text_align(u.shark_note, LV_TEXT_ALIGN_CENTER, 0);

    u.target = lv_obj_create(scr);
    lv_obj_remove_flag(u.target, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(u.target, rx, 48);
    lv_obj_set_size(u.target, rw, 34);
    lv_obj_set_style_radius(u.target, 6, 0);
    lv_obj_set_style_border_width(u.target, 0, 0);
    lv_obj_set_style_pad_all(u.target, 0, 0);
    lv_obj_t *t = label(u.target, &lv_font_montserrat_20, COL_BG, 0, 6, rw, false);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_user_data(u.target, t);
    lv_obj_t *lt = label(scr, &lv_font_montserrat_14, COL_TITLE, rx, 92, 0, false);
    lv_label_set_text(lt, "LOOK");
    u.look = label(scr, &lv_font_montserrat_16, COL_TEXT, rx, 108, rw, true);   // palette + layer, wraps
    lv_obj_set_height(u.look, 40);
    u.bri = label(scr, &lv_font_montserrat_14, COL_TITLE, rx, 152, rw, true);
    lv_obj_set_height(u.bri, 36);

    u.body = label(scr, &lv_font_montserrat_28, COL_TEXT, 8, 194, SCR_W - 16, false);
    u.eyes = label(scr, &lv_font_montserrat_16, COL_TEXT, 8, 230, (SCR_W - 24) / 2, false);
    u.fins = label(scr, &lv_font_montserrat_16, COL_TEXT, 16 + (SCR_W - 24) / 2, 230, (SCR_W - 24) / 2, false);
    u.foot = label(scr, &lv_font_montserrat_14, COL_TITLE, 8, 258, SCR_W - 16, false);
}

static void set_col(lv_obj_t *l, uint32_t c) { lv_obj_set_style_text_color(l, lv_color_hex(c), 0); }

static void fmt_preset(char *out, size_t n, const char *what, int preset, const char *name)
{
    if (preset <= 0) snprintf(out, n, "%s--", what);
    else if (name && name[0]) snprintf(out, n, "%s%d %s", what, preset, name);
    else snprintf(out, n, "%sPreset %d", what, preset);
}

static void ui_update(void)
{
    const int64_t now = esp_timer_get_time();
    nl_show_t s;
    taskENTER_CRITICAL(&lock);
    s = show;
    taskEXIT_CRITICAL(&lock);
    const bool heard = last_show_us && now - last_show_us < SHOW_LOST_MS * 1000LL;
    const bool stby = side == NL_SHOW_STANDBY;
    char text[64];

    if (!heard) {
        lv_label_set_text(u.title, stby ? "STANDBY" : "LIVE");
        lv_label_set_text(u.note, "no base");
    } else {
        if (s.brain < 0) lv_label_set_text(u.title, stby ? "STANDBY" : "LIVE");
        else lv_label_set_text_fmt(u.title, "%s %s", stby ? "STANDBY" : "LIVE", s.brain ? "L" : "R");
        const char *note = s.flags & NL_SHOW_LOCKED ? "LOCKED" : s.flags & NL_SHOW_LATCH ? "LATCH" : "";
        if (stby && (s.flags & NL_SHOW_PREVIEW)) note = "ON SHARK";
        lv_label_set_text(u.note, note);
    }

    fmt_preset(text, sizeof(text), "", s.body, s.body_name);
    lv_label_set_text(u.body, heard ? text : "--");
    fmt_preset(text, sizeof(text), "Eyes  ", s.eyes, s.eyes_name);
    lv_label_set_text(u.eyes, heard ? text : "Eyes  --");
    if (s.fins > 0) fmt_preset(text, sizeof(text), "Fins  ", s.fins, s.fins_name);
    else snprintf(text, sizeof(text), "Fins  %s", s.fins < 0 ? "--" : "follow body");
    lv_label_set_text(u.fins, heard ? text : "Fins  --");
    char pal[28] = "--";
    if (s.body_pal >= 0) {
        if (s.pal_name[0]) snprintf(pal, sizeof(pal), "%s", s.pal_name);
        else snprintf(pal, sizeof(pal), "pal %d", s.body_pal);
    }
    if (s.body_layer > 0) lv_label_set_text_fmt(u.look, "%s + L%d", pal, s.body_layer - NL_LAYER_PRESET_BASE);
    else lv_label_set_text(u.look, heard ? pal : "--");
    if (stby) {
        lv_label_set_text(u.bri, !heard ? "" : (s.flags & NL_SHOW_DIFFERS) ? "hold TAKE to show" : "same as Live");
        set_col(u.bri, (s.flags & NL_SHOW_DIFFERS) ? COL_STANDBY : COL_DIM);
    } else if (heard && (s.flags & NL_SHOW_WLED_HEARD)) {
        lv_label_set_text_fmt(u.bri, "Bright %d%%", (s.brightness * 100 + 127) / 255);
        set_col(u.bri, COL_TITLE);
    } else {
        lv_label_set_text(u.bri, heard ? "WLED not heard" : "");
        set_col(u.bri, COL_WARN);
    }
    // Standby that matches Live is dim; everything is dim without the base.
    const uint32_t c = !heard || (stby && !(s.flags & NL_SHOW_DIFFERS)) ? COL_DIM : COL_TEXT;
    set_col(u.body, c);
    set_col(u.eyes, c);
    set_col(u.fins, c);
    set_col(u.look, c);

    static const char *const names[] = { "ALL", "BODY", "EYES", "FINS" };
    static const uint32_t cols[] = { 0xffffff, COL_WLED, COL_EYES, COL_FINS };
    const int t = s.target < 4 ? s.target : 0;
    lv_label_set_text((lv_obj_t *)lv_obj_get_user_data(u.target), heard ? names[t] : "--");
    lv_obj_set_style_bg_color(u.target, lv_color_hex(!heard || (s.flags & NL_SHOW_LOCKED) ? COL_DIM : cols[t]), 0);

    nl_espnow_stats_t st;
    nl_espnow_get_stats(&st);
    lv_label_set_text_fmt(u.foot, "ch %d %s   %s", st.channel, st.locked ? "" : "(searching)",
                          last_rx_us && now - last_rx_us < STREAM_LOST_MS * 1000LL ? "picture ok" : "no picture");
}

// The shark from the stream: a dot per LED; dim grey without a stream.
static void shark_draw(bool stream)
{
    static uint8_t now[LED_POS_COUNT];
    static uint16_t lut[256];
    static bool lut_made;
    if (!lut_made) {
        for (int i = 0; i < 256; i++) {
            const lv_color_t c = lv_color_make((i >> 5) * 255 / 7, ((i >> 2) & 7) * 255 / 7, (i & 3) * 255 / 3);
            lut[i] = lv_color_to_u16(c);
        }
        lut_made = true;
    }
    if (stream) {
        taskENTER_CRITICAL(&lock);
        memcpy(now, incoming, sizeof(now));
        taskEXIT_CRITICAL(&lock);
    }
    const uint16_t dim = lv_color_to_u16(lv_color_hex(0x30343a));
    memset(shark_px, 0, SHARK_W * SHARK_H * 2);
    static const int8_t off[][2] = { {0,-1}, {-1,0}, {0,0}, {1,0}, {0,1} };
    for (int i = 0; i < LED_POS_COUNT; i++) {
        const uint16_t c = stream ? lut[now[i]] : dim;
        for (size_t k = 0; k < sizeof(off) / sizeof(off[0]); k++) {
            const int x = led_pos[i][0] + off[k][0], y = led_pos[i][1] + off[k][1];
            if (x >= 0 && x < SHARK_W && y >= 0 && y < SHARK_H) shark_px[y * SHARK_W + x] = c;
        }
    }
    lv_obj_invalidate(u.shark);
}

static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void ui_task(void *arg)
{
    int64_t next_ui = 0, last_log = 0;
    uint32_t drawn_gen = 0, drawn = 0;
    bool lit = false, was_stream = true;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        const bool stream = last_rx_us && now - last_rx_us < STREAM_LOST_MS * 1000LL;
        if (stream && frame_gen != drawn_gen) {
            drawn_gen = frame_gen;
            shark_draw(true);
            drawn++;
        } else if (!stream && was_stream) {
            shark_draw(false);
        }
        if (stream != was_stream) {
            lv_label_set_text(u.shark_note, stream ? "" : side == NL_SHOW_STANDBY ? "no Standby brain heard" : "no Live brain heard");
            was_stream = stream;
        }
        if (now >= next_ui) {
            next_ui = now + 250000;
            ui_update();
        }
        if (shot_wanted && !shot_filling) {
            shot_wanted = false;
            shot_filling = true;
            lv_obj_invalidate(lv_screen_active());
        }
        lv_timer_handler();
        if (!lit) {
            lit = true;
            param(0x51, (uint8_t[]){ BRIGHTNESS }, 1);
        }
        if (now - last_log >= 5000000) {
            ESP_LOGI(TAG, "%s: %s, %lu frames in, %lu lost, %lu drawn; base %s",
                     side == NL_SHOW_STANDBY ? "STANDBY" : "LIVE", stream ? "picture" : "NO PICTURE",
                     (unsigned long)rx_frames, (unsigned long)rx_lost, (unsigned long)drawn,
                     last_show_us && now - last_show_us < SHOW_LOST_MS * 1000LL ? "heard" : "NOT heard");
            drawn = 0;
            last_log = now;
        }
        const int64_t spent_ms = (esp_timer_get_time() - now) / 1000;
        vTaskDelay(pdMS_TO_TICKS(spent_ms < FRAME_MS ? FRAME_MS - spent_ms : 1));
    }
}

// ------------------------------------------------------------------ console

static void shot_print(void)
{
    const size_t n = SCR_W * SCR_H * 2;
    if (!shot_ready) shot_ready = xSemaphoreCreateBinary();
    if (!shot_buf) shot_buf = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (!shot_buf) return;
    xSemaphoreTake(shot_ready, 0);
    shot_wanted = true;
    if (xSemaphoreTake(shot_ready, pdMS_TO_TICKS(2000)) != pdTRUE) {
        shot_wanted = shot_filling = false;
        printf("SHOT failed\n");
        return;
    }
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    printf("SHOT %d %d rgb565le\n", SCR_W, SCR_H);
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

static void console_task(void *arg)
{
    char line[64];
    size_t n = 0;
    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) continue;
        if (c != '\n' && c != '\r') {
            if (n < sizeof(line) - 1) line[n++] = c;
            continue;
        }
        line[n] = 0;
        n = 0;
        if (!strcmp(line, "shot") || !strcmp(line, "shot rle")) shot_print();
        else if (!strcmp(line, "side live") || !strcmp(line, "side standby")) {
            nvs_handle_t h;
            const uint8_t v = line[5] == 's';
            if (nvs_open("monitor", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_u8(h, "side", v);
                nvs_commit(h);
                nvs_close(h);
            }
            printf("SIDE %s, restarting\n", v ? "standby" : "live");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        } else if (!strcmp(line, "flip")) {   // turn the picture round 180 degrees (saved)
            nvs_handle_t h;
            const uint8_t v = !flipped_from_settings();
            if (nvs_open("monitor", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_u8(h, "flip", v);
                nvs_commit(h);
                nvs_close(h);
            }
            printf("FLIP %s, restarting\n", v ? "on" : "off");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        } else if (line[0]) printf("? commands: side live|standby, flip, shot\n");
    }
}

static bool flipped_from_settings(void)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open("monitor", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "flip", &v);
        nvs_close(h);
    }
    return v;
}

static int side_from_settings(void)
{
    nvs_handle_t h;
    uint8_t v;
    if (nvs_open("monitor", NVS_READONLY, &h) == ESP_OK) {
        const esp_err_t err = nvs_get_u8(h, "side", &v);
        nvs_close(h);
        if (err == ESP_OK) return v ? NL_SHOW_STANDBY : NL_SHOW_LIVE;
    }
    static const uint8_t standby[][6] = MONITOR_STANDBY_MACS;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    for (size_t i = 0; i < sizeof(standby) / sizeof(standby[0]); i++)
        if (!memcmp(mac, standby[i], 6)) return NL_SHOW_STANDBY;
    return NL_SHOW_LIVE;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    side = side_from_settings();
    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb);

    sent = xSemaphoreCreateBinary();
    lcd_init();
    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_rotation(disp, flipped_from_settings() ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_90);
    const size_t bytes = SCR_W * BUF_ROWS * 2;   // bands of the landscape picture
    rot_buf = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint8_t *b1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint8_t *b2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(b1 && b2 && rot_buf);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, round_area, LV_EVENT_INVALIDATE_AREA, NULL);
    ui_build();
    shark_draw(false);

    const nl_espnow_config_t radio = {
        .role = NL_ROLE_PREVIEW, .side = NL_SIDE_NONE, .fw = 1, .anchor = false,
        .anchor_channel = START_CHANNEL, .anchor_fallback_ms = 0,   // follows the brains' channel
        .handler = on_radio, .core = 0,
    };
    ESP_ERROR_CHECK(nl_espnow_start(&radio));
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 2, NULL, 0);
    ESP_LOGI(TAG, "up: the %s monitor, %d LEDs on %d x %d", side == NL_SHOW_STANDBY ? "Standby" : "Live", LED_POS_COUNT, LCD_W, LCD_H);
}
