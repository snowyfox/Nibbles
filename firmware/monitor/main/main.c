// Live / Standby monitor (2026-11-14): a Waveshare ESP32-S3-Touch-AMOLED-1.64
// (CO5300, 280 x 456 on QSPI, portrait) in the base box. One shows Live, the
// other Standby: the side view of the shark from that side's brain
// (NL_MSG_PREVIEW) and, under it, what the base's switcher says that side is
// (NL_MSG_SHOW): body, eyes, fins, look. Radio only: no cable to the base.
// Which side: by MAC (MONITOR_STANDBY_MACS), or "side live|standby" over USB
// (saved). Two modes, chosen on the base (its MONITORS key, NL_SHOW_BY_BRAIN):
// by side (each shows Live or Standby, whichever brain that is) or by brain
// (each follows one brain, the Live monitor the Right Brain and the Standby one
// the Left unless "brain right|left" says otherwise, and shows which it is). "shot" prints the screen as the base's does (tools/base_shot.py).
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
#include "ota.h"
#include "ota_remote.h"
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
// The simple page's shark, bigger (2026-11-23): the same map scaled by 420/296.
#define BIG_W          420
#define BIG_H          213
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

static int side;      // NL_SHOW_LIVE or NL_SHOW_STANDBY: this monitor's side in the side mode
static int my_brain;  // 0 Right, 1 Left: the brain this monitor follows in the brain mode
static bool flipped_from_settings(void);
static void shark_draw(bool stream);

static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t sent;
static uint8_t *rot_buf;           // a band turned to the panel's orientation (DMA)

// From the radio task.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t incoming[LED_POS_COUNT];
static volatile int64_t last_rx_us, last_show_us;
static volatile uint32_t rx_frames, rx_lost, frame_gen;
static int last_frame = -1;
static nl_show_t shows[2];                 // the base's latest per side (NL_SHOW_LIVE, NL_SHOW_STANDBY)
static volatile int64_t show_us[2];
static volatile bool by_brain;            // the base's mode (NL_SHOW_BY_BRAIN)
static volatile int brain_side = -1;      // the brain mode: the side my brain is on (from its stream), -1 not heard
static volatile int64_t brain_us;
static volatile bool detail;              // the base's DETAIL key (NL_SHOW_DETAIL)

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
    if (ota_remote_radio(hdr, payload, len)) return;   // the base asks for Wi-Fi to take an update
    if (hdr->type == NL_MSG_SHOW && len == sizeof(nl_show_t)) {
        const nl_show_t *s = (const nl_show_t *)payload;
        if (s->side > NL_SHOW_STANDBY) return;
        taskENTER_CRITICAL(&lock);
        shows[s->side] = *s;
        taskEXIT_CRITICAL(&lock);
        show_us[s->side] = now;
        last_show_us = now;
        detail = s->flags & NL_SHOW_DETAIL;
        const bool mode = s->flags & NL_SHOW_BY_BRAIN;
        if (mode != by_brain) {
            by_brain = mode;
            last_frame = -1;   // another stream from now on
        }
        return;
    }
    if (hdr->type != NL_MSG_PREVIEW || len <= NL_PREVIEW_HEADER_LEN) return;
    // The Live brain sends as a WLED, the Standby one as NL_ROLE_WLED_STANDBY;
    // brains with an A/B side also say which they are (NL_PREVIEW_RIGHT, _LEFT).
    const nl_preview_t *m = (const nl_preview_t *)payload;
    const int from_side = hdr->role == NL_ROLE_WLED_STANDBY ? NL_SHOW_STANDBY : NL_SHOW_LIVE;
    if (by_brain) {
        if (nl_preview_brain(m->flags) != my_brain) return;
        if (brain_side != from_side) {
            brain_side = from_side;
            last_frame = -1;   // the same brain's frames, but don't count the swap as lost
        }
        brain_us = now;
    } else if (from_side != side) {
        return;
    }
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
    lv_obj_t *simple, *detail;   // big text (normal), small text (the base's DETAIL key)
    lv_obj_t *s_banner, *s_title, *s_note, *s_cap, *s_val, *s_body;
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

#define SHARK_Y_SIMPLE 62
#define SHARK_Y_DETAIL 42

static lv_obj_t *page_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_size(o, SCR_W, SCR_H);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

// Landscape, 456 x 280: the banner across the top; the shark left, the
// target and look on the right; the body preset big along the bottom, eyes
// and fins under it.
// The shark's canvas: big on the simple page, the old size on the detail page.
static bool shark_big;
static void shark_size(bool big)
{
    shark_big = big;
    lv_canvas_set_buffer(u.shark, shark_px, big ? BIG_W : SHARK_W, big ? BIG_H : SHARK_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(u.shark, big ? (SCR_W - BIG_W) / 2 : 4, big ? SHARK_Y_SIMPLE : SHARK_Y_DETAIL);
}

static void ui_build(void)
{
    lv_obj_t *scr0 = lv_screen_active();
    lv_obj_set_style_bg_color(scr0, lv_color_hex(COL_BG), 0);
    lv_obj_remove_flag(scr0, LV_OBJ_FLAG_SCROLLABLE);
    const uint32_t accent = side == NL_SHOW_STANDBY ? COL_STANDBY : COL_LIVE;
    const int rx = SHARK_W + 8, rw = SCR_W - rx - 6;   // the right column

    // The simple page (2026-11-14, read from half a metre): a 56 px banner
    // (LIVE / STANDBY, or the brain and its role), the shark, one big number
    // or word on the right, the body preset big along the bottom.
    u.simple = page_box(scr0);
    u.s_banner = page_box(u.simple);
    lv_obj_set_size(u.s_banner, SCR_W, 56);
    lv_obj_set_style_bg_opa(u.s_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(u.s_banner, lv_color_hex(accent), 0);
    // The preset's name across the top (2026-11-23; the banner's colour says Live or
    // Standby), the brightness or NEW / SAME / ON SHARK on the right.
    u.s_title = label(u.s_banner, &lv_font_montserrat_40, COL_BG, 10, 6, 330, false);
    u.s_note = label(u.s_banner, &lv_font_montserrat_28, COL_BG, 336, 13, SCR_W - 346, false);
    lv_obj_set_style_text_align(u.s_note, LV_TEXT_ALIGN_RIGHT, 0);
    u.s_cap = label(u.simple, &lv_font_montserrat_20, COL_TITLE, rx, 92, rw, false);   // not shown any more
    u.s_val = label(u.simple, &lv_font_montserrat_40, COL_TEXT, rx, 116, rw, false);
    u.s_body = label(u.simple, &lv_font_montserrat_40, COL_TEXT, 8, 222, SCR_W - 16, false);
    lv_obj_add_flag(u.s_cap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(u.s_val, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(u.s_body, LV_OBJ_FLAG_HIDDEN);

    // The detail page: the first layout.
    u.detail = page_box(scr0);
    lv_obj_add_flag(u.detail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *scr = u.detail;

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

    // The shark, on both pages (moved by page_show).
    shark_px = heap_caps_calloc(BIG_W * BIG_H, 2, MALLOC_CAP_SPIRAM);   // big enough for either size
    u.shark = lv_canvas_create(scr0);
    shark_size(true);
    u.shark_note = label(scr0, &lv_font_montserrat_20, COL_DIM, (SCR_W - BIG_W) / 2, SHARK_Y_SIMPLE + 2, BIG_W, false);   // above the shark's back
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

// The side to show now: this monitor's own in the side mode; in the brain mode
// the side its brain is on (from the brain's stream, else from the base's
// A/B brains), -1 while that isn't known.
static int shown_side(int64_t now)
{
    if (!by_brain) return side;
    if (brain_side >= 0 && now - brain_us < STREAM_LOST_MS * 1000LL) return brain_side;
    for (int i = 0; i < 2; i++)
        if (show_us[i] && now - show_us[i] < SHOW_LOST_MS * 1000LL && shows[i].brain == my_brain) return i;
    return -1;
}

static void ui_update(void)
{
    const int64_t now = esp_timer_get_time();
    const int vs = shown_side(now);
    nl_show_t s = { 0 };
    if (vs >= 0) {
        taskENTER_CRITICAL(&lock);
        s = shows[vs];
        taskEXIT_CRITICAL(&lock);
    }
    const bool base_heard = last_show_us && now - last_show_us < SHOW_LOST_MS * 1000LL;
    const bool heard = vs >= 0 && show_us[vs] && now - show_us[vs] < SHOW_LOST_MS * 1000LL;
    const bool stby = vs == NL_SHOW_STANDBY;
    const char *brain_name = my_brain ? "LEFT" : "RIGHT";
    char text[64];

    // The banner: red Live, green Standby, grey when this brain's side isn't known.
    static uint32_t banner_col;
    const uint32_t bc = vs < 0 ? COL_DIM : stby ? COL_STANDBY : COL_LIVE;
    if (bc != banner_col) {
        lv_obj_set_style_bg_color(u.banner, lv_color_hex(bc), 0);
        lv_obj_set_style_bg_color(u.s_banner, lv_color_hex(bc), 0);
        banner_col = bc;
    }
    // Which page: the base's DETAIL key (the simple page without the base).
    static int page = -1;
    const int want = base_heard && detail ? 1 : 0;
    if (want != page) {
        page = want;
        lv_obj_add_flag(want ? u.simple : u.detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(want ? u.detail : u.simple, LV_OBJ_FLAG_HIDDEN);
        shark_size(!want);
        shark_draw(false);   // redrawn at the new size; the stream fills it in again
        lv_obj_set_pos(u.shark_note, want ? 4 : (SCR_W - BIG_W) / 2, (want ? SHARK_Y_DETAIL : SHARK_Y_SIMPLE) + 2);
        lv_obj_set_width(u.shark_note, want ? SHARK_W : BIG_W);
        lv_obj_set_style_text_font(u.shark_note, want ? &lv_font_montserrat_14 : &lv_font_montserrat_20, 0);
    }
    const char *note = !heard ? "" : s.flags & NL_SHOW_LOCKED ? "LOCKED" : s.flags & NL_SHOW_LATCH ? "LATCH" : "";
    if (heard && stby && (s.flags & NL_SHOW_PREVIEW)) note = "ON SHARK";
    if (!base_heard) note = "no base";
    if (by_brain) {
        // "RIGHT  LIVE": the brain first, as it's what this screen is now.
        if (vs < 0) lv_label_set_text_fmt(u.title, "%s BRAIN", brain_name);
        else lv_label_set_text_fmt(u.title, "%s %s", brain_name, stby ? "STANDBY" : "LIVE");
        if (vs < 0 && base_heard) note = "not heard";
    } else if (!heard || s.brain < 0) {
        lv_label_set_text(u.title, stby ? "STANDBY" : "LIVE");
    } else {
        lv_label_set_text_fmt(u.title, "%s %s", stby ? "STANDBY" : "LIVE", s.brain ? "L" : "R");
    }
    lv_label_set_text(u.note, note);
    fmt_preset(text, sizeof(text), "", s.body, s.body_name);
    lv_label_set_text(u.body, heard ? text : "--");
    lv_label_set_text(u.s_body, heard ? text : "--");
    // The simple page's banner: the preset, or which brain this is while it isn't heard.
    if (heard) lv_label_set_text(u.s_title, text);
    else lv_label_set_text(u.s_title, lv_label_get_text(u.title));
    // The simple page's one number or word: Live's brightness; Standby: NEW
    // (TAKE would change something) or SAME.
    if (!heard) {
        lv_label_set_text(u.s_cap, "");
        lv_label_set_text(u.s_val, "");
    } else if (stby) {
        const bool differs = s.flags & NL_SHOW_DIFFERS;
        lv_label_set_text(u.s_cap, "vs LIVE");
        lv_label_set_text(u.s_val, differs ? "NEW" : "SAME");
        set_col(u.s_val, differs ? COL_STANDBY : COL_DIM);
    } else {
        lv_label_set_text(u.s_cap, "BRIGHT");
        if (s.flags & NL_SHOW_WLED_HEARD) {
            lv_label_set_text_fmt(u.s_val, "%d%%", (s.brightness * 100 + 127) / 255);
            set_col(u.s_val, COL_TEXT);
        } else {
            lv_label_set_text(u.s_val, "--");
            set_col(u.s_val, COL_DIM);
        }
    }
    // The banner's right end: LATCH / ON SHARK / no base if there's one, else the value.
    lv_label_set_text(u.s_note, note[0] || !heard ? note : lv_label_get_text(u.s_val));
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
    set_col(u.s_body, c);

    static const char *const names[] = { "ALL", "BODY", "EYES", "FINS" };
    static const uint32_t cols[] = { 0xffffff, COL_WLED, COL_EYES, COL_FINS };
    const int t = s.target < 4 ? s.target : 0;
    lv_label_set_text((lv_obj_t *)lv_obj_get_user_data(u.target), heard ? names[t] : "--");
    lv_obj_set_style_bg_color(u.target, lv_color_hex(!heard || (s.flags & NL_SHOW_LOCKED) ? COL_DIM : cols[t]), 0);

    nl_espnow_stats_t st;
    nl_espnow_get_stats(&st);
    char upd[48];
    ota_remote_text(upd, sizeof(upd));   // while the base has it on Wi-Fi: where to send the update
    if (upd[0]) {
        lv_label_set_text(u.foot, upd);
        set_col(u.foot, COL_WARN);
    } else {
        lv_label_set_text_fmt(u.foot, "ch %d %s   %s", st.channel, st.locked ? "" : "(searching)",
                              last_rx_us && now - last_rx_us < STREAM_LOST_MS * 1000LL ? "picture ok" : "no picture");
        set_col(u.foot, COL_TITLE);
    }
}

// The big shark, blended (2026-11-23, the user: no black spots between the LEDs).
// Each LED spreads over a disc of radius BLEND_R with a weight falling off from its
// centre; a pixel takes the weighted average of the LEDs over it, divided by at
// least BLEND_FULL so the outer edge of the shark fades instead of ending hard.
#define BLEND_R    4
#define BLEND_FULL 24
static void shark_draw_blended(const uint8_t *leds)
{
    static uint16_t *acc;    // r, g, b sums per pixel
    static uint16_t *wsum;
    static uint8_t wt[2 * BLEND_R + 1][2 * BLEND_R + 1];
    if (!acc) {
        acc = heap_caps_malloc((size_t)BIG_W * BIG_H * 3 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        wsum = heap_caps_malloc((size_t)BIG_W * BIG_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        for (int dy = -BLEND_R; dy <= BLEND_R; dy++)
            for (int dx = -BLEND_R; dx <= BLEND_R; dx++) {
                const int d2 = dx * dx + dy * dy, r2 = (BLEND_R + 1) * (BLEND_R + 1);
                wt[dy + BLEND_R][dx + BLEND_R] = d2 < r2 ? (uint8_t)(32 * (r2 - d2) / r2) : 0;
            }
        if (!acc || !wsum) return;
    }
    memset(acc, 0, (size_t)BIG_W * BIG_H * 3 * sizeof(uint16_t));
    memset(wsum, 0, (size_t)BIG_W * BIG_H * sizeof(uint16_t));
    for (int i = 0; i < LED_POS_COUNT; i++) {
        int r = 0x30, g = 0x34, b = 0x3a;   // dim grey without a stream
        if (leds) {
            const uint8_t v = leds[i];
            r = (v >> 5) * 255 / 7; g = ((v >> 2) & 7) * 255 / 7; b = (v & 3) * 255 / 3;
        }
        const int cx = led_pos[i][0] * BIG_W / SHARK_W, cy = led_pos[i][1] * BIG_H / SHARK_H;
        for (int dy = -BLEND_R; dy <= BLEND_R; dy++) {
            const int y = cy + dy;
            if (y < 0 || y >= BIG_H) continue;
            for (int dx = -BLEND_R; dx <= BLEND_R; dx++) {
                const int x = cx + dx, k = wt[dy + BLEND_R][dx + BLEND_R];
                if (!k || x < 0 || x >= BIG_W) continue;
                const int p = y * BIG_W + x;
                // r*k fits: 255 * 32 = 8160, and few discs overlap one pixel
                acc[p * 3] += (uint16_t)(r * k >> 3); acc[p * 3 + 1] += (uint16_t)(g * k >> 3); acc[p * 3 + 2] += (uint16_t)(b * k >> 3);
                wsum[p] += (uint16_t)k;
            }
        }
    }
    for (int p = 0; p < BIG_W * BIG_H; p++) {
        if (!wsum[p]) continue;
        const int d = wsum[p] < BLEND_FULL ? BLEND_FULL : wsum[p];
        const int r = (acc[p * 3] << 3) / d, g = (acc[p * 3 + 1] << 3) / d, b = (acc[p * 3 + 2] << 3) / d;
        shark_px[p] = lv_color_to_u16(lv_color_make(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b));
    }
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
    const int w = shark_big ? BIG_W : SHARK_W, h = shark_big ? BIG_H : SHARK_H;
    memset(shark_px, 0, (size_t)w * h * 2);
    if (shark_big) {   // blended (2026-11-23): each LED a soft round glow, overlaps averaged
        shark_draw_blended(stream ? now : NULL);
        lv_obj_invalidate(u.shark);
        return;
    }
    static const int8_t off[][2] = { {0,-1}, {-1,0}, {0,0}, {1,0}, {0,1} };
    static const int8_t off_big[][2] = { {0,-1}, {1,-1}, {-1,0}, {0,0}, {1,0}, {2,0}, {-1,1}, {0,1}, {1,1}, {2,1}, {0,2}, {1,2} };
    const int8_t (*o)[2] = shark_big ? off_big : off;
    const size_t no = shark_big ? sizeof(off_big) / sizeof(off_big[0]) : sizeof(off) / sizeof(off[0]);
    for (int i = 0; i < LED_POS_COUNT; i++) {
        const uint16_t c = stream ? lut[now[i]] : dim;
        const int px = shark_big ? led_pos[i][0] * BIG_W / SHARK_W : led_pos[i][0];
        const int py = shark_big ? led_pos[i][1] * BIG_H / SHARK_H : led_pos[i][1];
        for (size_t k = 0; k < no; k++) {
            const int x = px + o[k][0], y = py + o[k][1];
            if (x >= 0 && x < w && y >= 0 && y < h) shark_px[y * w + x] = c;
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
            was_stream = stream;
        }
        {
            static bool was_mode;
            static const char *shown_note;
            const char *n = stream ? "" : by_brain ? (my_brain ? "Left Brain not heard" : "Right Brain not heard")
                          : side == NL_SHOW_STANDBY ? "no Standby brain heard" : "no Live brain heard";
            if (n != shown_note) {
                lv_label_set_text(u.shark_note, n);
                shown_note = n;
            }
            if (by_brain != was_mode) {   // the other stream: clear the picture until it comes
                was_mode = by_brain;
                taskENTER_CRITICAL(&lock);
                memset(incoming, 0, sizeof(incoming));
                taskEXIT_CRITICAL(&lock);
                shark_draw(false);
            }
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
            const int vs = shown_side(now);
            ESP_LOGI(TAG, "%s%s%s: %s, %lu frames in, %lu lost, %lu drawn; base %s",
                     by_brain ? (my_brain ? "LEFT BRAIN, " : "RIGHT BRAIN, ") : "",
                     vs < 0 ? "side not known" : vs == NL_SHOW_STANDBY ? "STANDBY" : "LIVE", by_brain ? "" : " (by side)",
                     stream ? "picture" : "NO PICTURE",
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

static void console_reply(void *ctx, const char *text) { printf("%s\n", text); }

static void console_task(void *arg)
{
    static char line[OTA_LINE_MAX + 64];   // "ota data" lines (tools/base_ota.py usb PORT BIN) are about 1 KB
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
        if (ota_command(line, console_reply, NULL)) continue;
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
        } else if (!strcmp(line, "brain right") || !strcmp(line, "brain left")) {   // the brain mode's brain (saved)
            nvs_handle_t h;
            const uint8_t v = line[6] == 'l';
            if (nvs_open("monitor", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_u8(h, "brain", v);
                nvs_commit(h);
                nvs_close(h);
            }
            my_brain = v;
            printf("BRAIN %s\n", v ? "left" : "right");
        } else if (line[0]) printf("? commands: side live|standby, brain right|left, flip, shot, ota status|...\n");
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

// The brain mode's brain: saved ("brain right|left"), else the Live monitor
// (the top one) follows the Left Brain and the Standby one the Right.
static int brain_from_settings(void)
{
    nvs_handle_t h;
    uint8_t v;
    if (nvs_open("monitor", NVS_READONLY, &h) == ESP_OK) {
        const esp_err_t err = nvs_get_u8(h, "brain", &v);
        nvs_close(h);
        if (err == ESP_OK) return v ? 1 : 0;
    }
    // Swapped 2026-11-23 (the user): the Live-by-MAC board is the top monitor, and
    // the top one is the Left Brain's.
    return side == NL_SHOW_STANDBY ? 0 : 1;
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
    my_brain = brain_from_settings();
    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb.rx_buffer_size = 4096;   // whole "ota data" lines without dropping bytes
    usb_serial_jtag_driver_install(&usb);
    const char *name = side == NL_SHOW_STANDBY ? "Standby monitor" : "Live monitor";
    ota_set_name(name, "nibbles_monitor.bin");
    ota_start();   // a new image is kept once it has run 30 s

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
    ota_wifi_init();   // the Wi-Fi netif before the radio starts Wi-Fi
    ESP_ERROR_CHECK(nl_espnow_start(&radio));
    ota_remote_start(NL_OTA_TO_MONITORS, name);
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 2, NULL, 0);
    ESP_LOGI(TAG, "up: the %s monitor, %d LEDs on %d x %d", side == NL_SHOW_STANDBY ? "Standby" : "Live", LED_POS_COUNT, LCD_W, LCD_H);
}
