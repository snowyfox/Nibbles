// The base's screen on a Waveshare ESP32-S3-AMOLED-1.91 (RM67162, 240 x 536
// over QSPI, mounted in portrait; no touch), with LVGL (2026-11-14). The same
// API as the 3.49's display.c (display.h), but one page: power at the top,
// the keypad's target, Live and Standby by name, the radio and log under it. The
// sharks, fins and eyes go to the Live and Standby monitors (1.64 boards);
// the touch actions, palette viewer and survey page don't exist here.
#include "display.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "power_math.h"

static const char *TAG = "display";

// The 1.91's panel wiring (as firmware/preview, which has run on this board).
#define LCD_HOST       SPI2_HOST
#define PIN_CS         6
#define PIN_SCLK       47
#define PIN_D0         18
#define PIN_D1         7
#define PIN_D2         48
#define PIN_D3         5
#define PIN_RST        17
#define LCD_W          240         // portrait: the panel's own orientation
#define LCD_H          536
#define BUF_ROWS       20          // LVGL draws in bands this tall, two buffers (internal RAM: Bluetooth needs its share)
#define BRIGHTNESS     0xD0        // 0..255 (register 0x51)
#define UPDATE_MS      250
#define SIMPLE_EVENT_MS 8000       // the simple page shows an event this long

#define CMD(c)         ((0x02u << 24) | ((uint32_t)(c) << 8))     // command and parameters on one line
#define COLOR_CMD(c)   ((0x32u << 24) | ((uint32_t)(c) << 8))     // pixels on four

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
#define COL_FINS    0xff8000
#define COL_LIVE    0xff3030
#define COL_STANDBY 0x30e060

static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t sent;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static display_status_t status;
static char event_text[64];
static bool event_new;
static volatile int page_wanted = -1;

// Screenshots ("shot"): the bands of one whole redraw copied into a frame.
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

static esp_err_t panel_init(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCLK, .data0_io_num = PIN_D0, .data1_io_num = PIN_D1,
        .data2_io_num = PIN_D2, .data3_io_num = PIN_D3,
        .max_transfer_sz = LCD_W * BUF_ROWS * 2 + 16,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");
    const esp_lcd_panel_io_spi_config_t cfg = {
        .cs_gpio_num = PIN_CS, .dc_gpio_num = -1, .spi_mode = 0, .pclk_hz = 40000000,
        .trans_queue_depth = 4, .on_color_trans_done = on_sent,
        .lcd_cmd_bits = 32, .lcd_param_bits = 8, .flags.quad_mode = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_HOST, &cfg, &io), TAG, "panel io");

    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    param(0x11, NULL, 0);                          // sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    param(0x3A, (uint8_t[]){ 0x55 }, 1);           // 16 bits a pixel
    param(0x36, (uint8_t[]){ DISPLAY_191_UPSIDE_DOWN ? 0xC0 : 0x00 }, 1);   // portrait, either way up
    param(0x51, (uint8_t[]){ 0 }, 1);
    param(0x29, NULL, 0);                          // display on
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

// The RM67162 takes windows with even start and odd end columns and rows.
static void round_area(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
    if (a->x2 >= LCD_W) a->x2 = LCD_W - 1;
    if (a->y2 >= LCD_H) a->y2 = LCD_H - 1;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px)
{
    const int w = lv_area_get_width(a), h = lv_area_get_height(a);
    if (shot_filling && shot_buf) {
        for (int y = 0; y < h; y++) memcpy(shot_buf + ((a->y1 + y) * LCD_W + a->x1) * 2, px + y * w * 2, w * 2);
    }
    lv_draw_sw_rgb565_swap(px, w * h);   // the panel takes the high byte first
    param(0x2A, (uint8_t[]){ a->x1 >> 8, a->x1 & 255, a->x2 >> 8, a->x2 & 255 }, 4);
    param(0x2B, (uint8_t[]){ a->y1 >> 8, a->y1 & 255, a->y2 >> 8, a->y2 & 255 }, 4);
    esp_lcd_panel_io_tx_color(io, COLOR_CMD(0x2C), px, w * h * 2);
    xSemaphoreTake(sent, portMAX_DELAY);
    if (shot_filling && lv_display_flush_is_last(disp)) {
        shot_filling = false;
        xSemaphoreGive(shot_ready);
    }
    lv_display_flush_ready(disp);
}

// ------------------------------------------------------------------ UI

static lv_obj_t *label_at(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_size(l, w, lv_font_get_line_height(font));   // one line: cut off with "..."
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    lv_label_set_text(l, "");
    return l;
}

static void set_col(lv_obj_t *l, uint32_t c) { lv_obj_set_style_text_color(l, lv_color_hex(c), 0); }

static struct {
    lv_obj_t *simple, *detail;   // the two pages: big text (normal), small text (DETAIL)
    // The simple page
    lv_obj_t *s_pct, *s_icon, *s_bar, *s_cap1, *s_val1, *s_cap2, *s_val2, *s_problem, *s_problem_t;
    lv_obj_t *s_target, *s_target_name, *s_target_note, *s_warn, *s_event;
    // The detail page
    lv_obj_t *power, *pct, *pack_v, *chg, *input, *runtime, *lights, *temps, *problem;
    lv_obj_t *target, *target_name, *page, *latch;
    lv_obj_t *live_t, *live_body, *live_eyes, *live_fins, *stby_t, *stby_body, *stby_eyes, *stby_fins;
    lv_obj_t *links, *log, *event;
    lv_obj_t *graph;
} m;

// The pack's charge over the last hour, as the 3.49's: 172 columns of ~21 s.
#define GRAPH_W        DISPLAY_GRAPH_COLUMNS
#define GRAPH_H        60
#define GRAPH_SLICE_MS (3600000 / GRAPH_W)
static uint16_t *graph_px;
static int8_t graph_pct[GRAPH_W], graph_restored[GRAPH_W];
static volatile bool graph_restore_ready;
static int64_t graph_next_us;

void display_graph_restore(const int8_t pct[DISPLAY_GRAPH_COLUMNS])
{
    memcpy(graph_restored, pct, GRAPH_W);
    graph_restore_ready = true;
}

static void graph_update(int pct)
{
    if (!graph_px) return;
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
        const uint16_t col = level == PACK_CRITICAL ? 0xF800 : level == PACK_LOW ? 0xFD20 : 0x07E0;
        for (int y = GRAPH_H - (v * GRAPH_H + 50) / 100; y < GRAPH_H; y++) graph_px[y * GRAPH_W + x] = col;
    }
    lv_obj_invalidate(m.graph);
}

// A plain rectangle (no padding, no scrolling), for boxes and pages.
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, lv_opa_t opa, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

// The simple page (2026-11-14, the user: read from half a metre or more, in
// the dark, no touch): nothing smaller than 20 px, the numbers 40-48 px. The
// pack's charge and a bar; time left (or what charging does); the lights'
// watts; the worst problem in a red box; the keypad's target; only what's
// missing; the last event for a few seconds. Everything else: DETAIL.
static void build_simple(lv_obj_t *pg)
{
    const int w = LCD_W - 16;
    m.s_pct = label_at(pg, &lv_font_montserrat_48, COL_TEXT, 8, 2, 0);
    m.s_icon = label_at(pg, &lv_font_montserrat_48, COL_OK, 0, 2, 0);
    lv_obj_align(m.s_icon, LV_ALIGN_TOP_RIGHT, -8, 2);
    m.s_bar = lv_bar_create(pg);
    lv_obj_set_pos(m.s_bar, 8, 60);
    lv_obj_set_size(m.s_bar, w, 30);
    lv_bar_set_range(m.s_bar, 0, 100);
    lv_obj_set_style_bg_color(m.s_bar, lv_color_hex(COL_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m.s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(m.s_bar, lv_color_hex(COL_TITLE), LV_PART_MAIN);
    lv_obj_set_style_border_width(m.s_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(m.s_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(m.s_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(m.s_bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(m.s_bar, 0, LV_PART_MAIN);

    m.s_cap1 = label_at(pg, &lv_font_montserrat_20, COL_TITLE, 8, 100, w);
    m.s_val1 = label_at(pg, &lv_font_montserrat_40, COL_TEXT, 8, 122, w);
    m.s_cap2 = label_at(pg, &lv_font_montserrat_20, COL_TITLE, 8, 176, w);
    m.s_val2 = label_at(pg, &lv_font_montserrat_40, COL_TEXT, 8, 198, w);
    lv_label_set_text(m.s_cap2, "LIGHTS");

    m.s_problem = box(pg, 8, 254, w, 72, COL_BAD, LV_OPA_COVER, 8);
    m.s_problem_t = label_at(m.s_problem, &lv_font_montserrat_28, COL_TEXT, 8, 2, w - 16);
    lv_label_set_long_mode(m.s_problem_t, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(m.s_problem_t, 68);

    m.s_target = box(pg, 8, 336, w, 96, COL_DIM, LV_OPA_COVER, 10);
    m.s_target_name = label_at(m.s_target, &lv_font_montserrat_48, COL_BG, 0, 4, w);
    lv_obj_set_style_text_align(m.s_target_name, LV_TEXT_ALIGN_CENTER, 0);
    m.s_target_note = label_at(m.s_target, &lv_font_montserrat_28, COL_BG, 0, 60, w);
    lv_obj_set_style_text_align(m.s_target_note, LV_TEXT_ALIGN_CENTER, 0);

    m.s_warn = label_at(pg, &lv_font_montserrat_20, COL_WARN, 8, 440, w);
    m.s_event = label_at(pg, &lv_font_montserrat_20, COL_TEXT, 8, 466, w);   // three lines, to the bottom
    lv_label_set_long_mode(m.s_event, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(m.s_event, LCD_H - 466);
}

static void ui_build(void)
{
    lv_obj_t *scr0 = lv_screen_active();
    lv_obj_set_style_bg_color(scr0, lv_color_hex(COL_BG), 0);
    lv_obj_remove_flag(scr0, LV_OBJ_FLAG_SCROLLABLE);
    m.simple = box(scr0, 0, 0, LCD_W, LCD_H, COL_BG, LV_OPA_TRANSP, 0);
    build_simple(m.simple);
    // The detail page: the first layout, small text, while DETAIL is on.
    m.detail = box(scr0, 0, 0, LCD_W, LCD_H, COL_BG, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(m.detail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *scr = m.detail;
    const int w = LCD_W - 16;   // 8 px each side

    // Top: power (0..219), framed in red when there's a problem
    m.power = lv_obj_create(scr);
    lv_obj_remove_flag(m.power, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(m.power, 0, 0);
    lv_obj_set_size(m.power, LCD_W, 220);
    lv_obj_set_style_bg_color(m.power, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(m.power, 0, 0);
    lv_obj_set_style_radius(m.power, 0, 0);
    lv_obj_set_style_pad_all(m.power, 0, 0);
    lv_obj_set_style_outline_pad(m.power, -2, 0);
    lv_obj_set_style_outline_color(m.power, lv_color_hex(COL_BAD), 0);

    m.pct = label_at(m.power, &lv_font_montserrat_40, COL_TEXT, 8, 4, 0);
    m.pack_v = label_at(m.power, &lv_font_montserrat_20, COL_TEXT, 132, 6, 100);
    m.chg = label_at(m.power, &lv_font_montserrat_14, COL_TITLE, 132, 30, 100);
    m.input = label_at(m.power, &lv_font_montserrat_16, COL_TEXT, 8, 56, w);
    m.lights = label_at(m.power, &lv_font_montserrat_20, COL_TEXT, 8, 78, w);
    m.runtime = label_at(m.power, &lv_font_montserrat_20, COL_TEXT, 8, 104, w);
    m.temps = label_at(m.power, &lv_font_montserrat_16, COL_TITLE, 8, 130, w);
    m.problem = label_at(m.power, &lv_font_montserrat_16, COL_BAD, 8, 130, w);

    graph_px = heap_caps_calloc(GRAPH_W * GRAPH_H, 2, MALLOC_CAP_SPIRAM);
    memset(graph_pct, -1, sizeof(graph_pct));
    if (graph_px) {
        m.graph = lv_canvas_create(m.power);
        lv_canvas_set_buffer(m.graph, graph_px, GRAPH_W, GRAPH_H, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(m.graph, 8, 154);
        lv_obj_set_style_opa(m.graph, 128, 0);
        lv_obj_t *g = label_at(m.power, &lv_font_montserrat_12, COL_DIM, 8 + GRAPH_W + 6, 154 + GRAPH_H - 30, 50);
        lv_label_set_long_mode(g, LV_LABEL_LONG_WRAP);
        lv_obj_set_height(g, 30);
        lv_label_set_text(g, "last\nhour");
    }

    // The keypad's target
    m.target = lv_obj_create(scr);
    lv_obj_remove_flag(m.target, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(m.target, 8, 226);
    lv_obj_set_size(m.target, w, 48);
    lv_obj_set_style_border_width(m.target, 0, 0);
    lv_obj_set_style_radius(m.target, 8, 0);
    lv_obj_set_style_pad_all(m.target, 0, 0);
    m.target_name = label_at(m.target, &lv_font_montserrat_28, COL_BG, 10, 9, 0);
    m.page = label_at(m.target, &lv_font_montserrat_14, COL_BG, 116, 6, 104);
    m.latch = label_at(m.target, &lv_font_montserrat_14, COL_BG, 116, 26, 104);

    // Live and Standby, by name
    m.live_t = label_at(scr, &lv_font_montserrat_14, COL_LIVE, 8, 282, 0);
    m.live_body = label_at(scr, &lv_font_montserrat_20, COL_TEXT, 8, 298, w);
    m.live_eyes = label_at(scr, &lv_font_montserrat_14, COL_TITLE, 8, 322, w);
    m.live_fins = label_at(scr, &lv_font_montserrat_14, COL_TITLE, 8, 340, w);
    m.stby_t = label_at(scr, &lv_font_montserrat_14, COL_STANDBY, 8, 366, 0);
    m.stby_body = label_at(scr, &lv_font_montserrat_20, COL_TEXT, 8, 382, w);
    m.stby_eyes = label_at(scr, &lv_font_montserrat_14, COL_TITLE, 8, 406, w);
    m.stby_fins = label_at(scr, &lv_font_montserrat_14, COL_TITLE, 8, 424, w);

    // Links, keypads, radio; the log; the last event
    m.links = label_at(scr, &lv_font_montserrat_14, COL_TITLE, 8, 454, w);
    m.log = label_at(scr, &lv_font_montserrat_12, COL_TITLE, 8, 476, w);
    m.event = label_at(scr, &lv_font_montserrat_12, COL_TEXT, 8, 494, w);
    lv_label_set_long_mode(m.event, LV_LABEL_LONG_WRAP);   // up to three lines, to the bottom
    lv_obj_set_height(m.event, LCD_H - 494);
}

static uint32_t target_col(int t)
{
    static const uint32_t cols[] = { 0xffffff, COL_WLED, COL_EYES, COL_FINS, 0x9040ff };   // BODY+FINS violet
    return t >= 0 && t <= 4 ? cols[t] : COL_DIM;
}

static void fmt_preset(char *out, size_t n, int preset, const char *name)
{
    if (preset <= 0) snprintf(out, n, "--");
    else if (name && name[0]) snprintf(out, n, "%d %s", preset, name);
    else snprintf(out, n, "Preset %d", preset);
}

static void fill_side(lv_obj_t *body, lv_obj_t *eyes, lv_obj_t *fins, const ctl_scene_t *sc,
                      const char *body_name, const char *eyes_name, const char *fins_name)
{
    char text[48], p[40];
    fmt_preset(text, sizeof(text), sc->body, body_name);
    lv_label_set_text(body, text);
    fmt_preset(p, sizeof(p), sc->eyes, eyes_name);
    lv_label_set_text_fmt(eyes, "Eyes  %s", p);
    if (sc->fins > 0) fmt_preset(p, sizeof(p), sc->fins, fins_name);
    lv_label_set_text_fmt(fins, "Fins  %s", sc->fins < 0 ? "--" : sc->fins ? p : "follow body");
}

// The worst power problem as a line of text, or NULL.
static const char *power_problem(const power_status_t *p, char *buf, size_t n)
{
    if (p->have_charger && !p->charger_ok) return "Charger NOT set up";
    if (p->pole_off) return NULL;
    if (p->lights_off) return "LIGHTS SWITCHED OFF";
    if (p->have_expander && !p->efuse_pg) return "eFuse tripped / off";
    if (p->fault0 || p->fault1) {
        snprintf(buf, n, "Charger fault %02x %02x", p->fault0, p->fault1);
        return buf;
    }
    if (p->ts_flags & 0x09) return (p->ts_flags & 1) ? "Cells too hot to charge" : "Cells too cold to charge";
    return NULL;
}

static void simple_update(const display_status_t *s)
{
    const power_status_t *p = &s->power;
    const bool board = p->have_charger || p->have_monitor || p->have_expander;
    const chg_state_t cs = (chg_state_t)p->chg_state;
    const bool charging = p->docked && p->have_charger && p->charger_ok && cs != CHG_DONE && cs != CHG_NOT_CHARGING;

    // The pack: percent, the bar, and an icon: a bolt charging, the plug on a supply.
    if (p->pack_pct >= 0) {
        const pack_level_t lv = pack_level(p->pack_pct);
        const uint32_t col = lv == PACK_CRITICAL ? COL_BAD : lv == PACK_LOW ? COL_WARN : charging ? COL_OK : COL_TEXT;
        lv_label_set_text_fmt(m.s_pct, "%d%%", p->pack_pct);
        set_col(m.s_pct, col);
        lv_bar_set_value(m.s_bar, p->pack_pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(m.s_bar, lv_color_hex(lv == PACK_CRITICAL ? COL_BAD : lv == PACK_LOW ? COL_WARN : COL_OK), LV_PART_INDICATOR);
    } else {
        lv_label_set_text(m.s_pct, board ? "--%" : "???");
        set_col(m.s_pct, COL_DIM);
        lv_bar_set_value(m.s_bar, 0, LV_ANIM_OFF);
    }
    lv_label_set_text(m.s_icon, charging ? LV_SYMBOL_CHARGE : p->docked ? LV_SYMBOL_USB : "");
    set_col(m.s_icon, charging ? COL_OK : COL_TITLE);

    // Time left on the pack, or what charging is doing.
    if (p->docked && p->have_charger) {
        if (charging) {
            const int a10 = ((p->ibat_ma > 0 ? p->ibat_ma : 0) + 50) / 100;
            lv_label_set_text(m.s_cap1, "CHARGING");
            lv_label_set_text_fmt(m.s_val1, "%d.%d A", a10 / 10, a10 % 10);
            set_col(m.s_val1, COL_OK);
        } else {
            lv_label_set_text(m.s_cap1, "PLUGGED IN");
            lv_label_set_text(m.s_val1, cs == CHG_DONE ? "Full" : p->starved ? "Weak" : "Idle");
            set_col(m.s_val1, cs == CHG_DONE ? COL_OK : COL_WARN);
        }
    } else {
        lv_label_set_text(m.s_cap1, "TIME LEFT");
        if (p->runtime_min >= 0) {
            const int mins = p->runtime_min < 99 * 60 + 59 ? p->runtime_min : 99 * 60 + 59;
            lv_label_set_text_fmt(m.s_val1, "%dh %02dm", mins / 60, mins % 60);
            set_col(m.s_val1, p->runtime_min > 45 ? COL_TEXT : COL_WARN);
        } else {
            lv_label_set_text(m.s_val1, "--");
            set_col(m.s_val1, COL_DIM);
        }
    }

    // The lights.
    if (p->pole_off) {
        lv_label_set_text(m.s_val2, "POLE OFF");
        set_col(m.s_val2, COL_WARN);
    } else if (p->have_monitor) {
        lv_label_set_text_fmt(m.s_val2, "%d W", (p->light_mw > 0 ? p->light_mw + 500 : 0) / 1000);
        set_col(m.s_val2, COL_TEXT);
    } else {
        lv_label_set_text(m.s_val2, "--");
        set_col(m.s_val2, COL_DIM);
    }

    // The worst problem.
    char ptext[40];
    const char *problem = board ? power_problem(p, ptext, sizeof(ptext)) : "No power board";
    if (problem) {
        lv_label_set_text(m.s_problem_t, problem);
        lv_obj_remove_flag(m.s_problem, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m.s_problem, LV_OBJ_FLAG_HIDDEN);
    }

    // The keypad's target, and LOCKED, LATCH or what the numbers are.
    lv_obj_set_style_bg_color(m.s_target, lv_color_hex(s->keys_locked ? COL_DIM : target_col(s->target)), 0);
    const char *tname = ctl_target_name((ctl_target_t)s->target);
    lv_obj_set_style_text_font(m.s_target_name, strlen(tname) > 6 ? &lv_font_montserrat_40 : &lv_font_montserrat_48, 0);   // BODY+FINS
    lv_label_set_text(m.s_target_name, tname);
    // The one-handed layout's modes: SET or BUMP, and what the numbers are.
    char note[24];
    snprintf(note, sizeof(note), "%s %s", s->bump_mode ? "BUMP" : "SET", ctl_page_name(s->page));
    lv_label_set_text(m.s_target_note, s->keys_locked ? "LOCKED" : note);

    // Only what's missing.
    const char *miss[3];
    int nm = 0;
    if (!s->wled_fresh) miss[nm++] = "WLED";
    if (!s->eyes_fresh) miss[nm++] = "eyes";
    if (!s->keypads_found) miss[nm++] = "keys";
    char warn[40] = "";
    if (nm) snprintf(warn, sizeof(warn), "No %s%s%s%s%s", miss[0], nm > 1 ? ", " : "", nm > 1 ? miss[1] : "",
                     nm > 2 ? ", " : "", nm > 2 ? miss[2] : "");
    lv_label_set_text(m.s_warn, warn);
}

static void ui_update(const display_status_t *s)
{
    const power_status_t *p = &s->power;
    char text[96];
    const bool board = p->have_charger || p->have_monitor || p->have_expander;
    const chg_state_t cs = (chg_state_t)p->chg_state;
    const bool charging = p->docked && p->have_charger && p->charger_ok && cs != CHG_DONE && cs != CHG_NOT_CHARGING;
    graph_update(p->pack_mv ? p->pack_pct : -1);

    // The pack: charge, voltage, what the charger is doing.
    if (p->pack_pct >= 0) {
        lv_label_set_text_fmt(m.pct, "%d%%", p->pack_pct);
        const pack_level_t lv = pack_level(p->pack_pct);
        set_col(m.pct, lv == PACK_CRITICAL ? COL_BAD : lv == PACK_LOW ? COL_WARN : charging ? COL_OK : COL_TEXT);
        lv_label_set_text_fmt(m.pack_v, "%d.%02d V", p->pack_mv / 1000, p->pack_mv % 1000 / 10);
    } else {
        lv_label_set_text(m.pct, board ? "--%" : "???");
        set_col(m.pct, COL_DIM);
        lv_label_set_text(m.pack_v, "--.-- V");
    }
    if (!p->have_charger) lv_label_set_text(m.chg, board ? "no charger" : "no board");
    else if (charging) lv_label_set_text_fmt(m.chg, LV_SYMBOL_CHARGE " %s %d.%dA", bq_charge_state_name(cs),
                                             (p->ibat_ma > 0 ? p->ibat_ma : 0) / 1000, (p->ibat_ma > 0 ? p->ibat_ma : 0) % 1000 / 100);
    else lv_label_set_text(m.chg, p->docked ? bq_charge_state_name(cs) : "on the pack");

    // The LEMO input.
    if (!p->have_charger || p->input == INPUT_NONE) {
        lv_label_set_text(m.input, "In: nothing plugged in");
        set_col(m.input, COL_DIM);
    } else {
        const int v10 = (p->vbus_mv + 50) / 100, a10 = (p->in_ma + 50) / 100;
        if (p->input == INPUT_SUPPLY) snprintf(text, sizeof(text), "In: " LV_SYMBOL_USB " %d.%dV %d.%dA", v10 / 10, v10 % 10, a10 / 10, a10 % 10);
        else snprintf(text, sizeof(text), "In: battery %d%%  %d.%dV %d.%dA", p->input_pct, v10 / 10, v10 % 10, a10 / 10, a10 % 10);
        lv_label_set_text(m.input, text);
        set_col(m.input, p->starved ? COL_WARN : COL_TEXT);
    }

    // The lights.
    if (p->pole_off) {
        lv_label_set_text(m.lights, "Lights: POLE OFF");
        set_col(m.lights, COL_WARN);
    } else if (p->have_monitor) {
        const int ma = p->light_ma > 0 ? p->light_ma : 0, a10 = (ma + 50) / 100, w = (p->light_mw + 500) / 1000;
        lv_label_set_text_fmt(m.lights, "Lights: %d.%dA  %dW", a10 / 10, a10 % 10, w);
        set_col(m.lights, COL_TEXT);
    } else {
        lv_label_set_text(m.lights, "Lights: -.-A --W");
        set_col(m.lights, COL_DIM);
    }

    // Time left on the pack.
    if (p->runtime_min >= 0) {
        const int mins = p->runtime_min < 99 * 60 + 59 ? p->runtime_min : 99 * 60 + 59;
        lv_label_set_text_fmt(m.runtime, "Left: %dh %02dm", mins / 60, mins % 60);
        set_col(m.runtime, p->runtime_min > 45 ? COL_TEXT : COL_WARN);
    } else {
        lv_label_set_text(m.runtime, "Left: --");
        set_col(m.runtime, COL_DIM);
    }

    // Temperatures, or the worst problem in their place with a red border.
    char ptext[40];
    const char *problem = power_problem(p, ptext, sizeof(ptext));
    if (problem) {
        lv_label_set_text(m.problem, problem);
        lv_obj_remove_flag(m.problem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m.temps, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m.problem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m.temps, LV_OBJ_FLAG_HIDDEN);
        char c[12] = "--", d[12] = "--";
        if (p->have_charger && p->cell_c10 != INT16_MIN) snprintf(c, sizeof(c), "%d", (p->cell_c10 + 5) / 10);
        if (p->have_charger && p->die_c10 != INT16_MIN) snprintf(d, sizeof(d), "%d", (p->die_c10 + 5) / 10);
        lv_label_set_text_fmt(m.temps, "Cells %s°C   Charger %s°C", c, d);
        const temp_level_t t = p->have_charger ? (cell_temp_level(p->cell_c10) > die_temp_level(p->die_c10)
                                                 ? cell_temp_level(p->cell_c10) : die_temp_level(p->die_c10)) : TEMP_OK;
        set_col(m.temps, t == TEMP_BAD ? COL_BAD : t == TEMP_WARN ? COL_WARN : COL_TITLE);
    }
    lv_obj_set_style_outline_width(m.power, problem ? 2 : 0, 0);

    // The keypad's target.
    lv_obj_set_style_bg_color(m.target, lv_color_hex(s->keys_locked ? COL_DIM : target_col(s->target)), 0);
    lv_label_set_text(m.target_name, ctl_target_name((ctl_target_t)s->target));
    lv_label_set_text(m.page, ctl_page_name(s->page));
    lv_label_set_text(m.latch, s->keys_locked ? "LOCKED" : s->latch ? "LATCH" : "");

    // Live and Standby, by name.
    lv_label_set_text(m.live_t, s->ab_live < 0 ? "LIVE" : s->ab_live ? "LIVE  L" : "LIVE  R");
    fill_side(m.live_body, m.live_eyes, m.live_fins, &s->live, s->live_body, s->live_eyes, s->live_fins);
    set_col(m.live_body, s->wled_fresh ? COL_TEXT : COL_DIM);
    lv_label_set_text(m.stby_t, s->take_preview ? "STANDBY  (PREVIEW)" : s->ab_live < 0 ? "STANDBY" : s->ab_live ? "STANDBY  R" : "STANDBY  L");
    fill_side(m.stby_body, m.stby_eyes, m.stby_fins, &s->standby, s->stby_body, s->stby_eyes, s->stby_fins);
    set_col(m.stby_body, s->diff ? COL_TEXT : COL_DIM);

    // Links, keypads, radio, log.
    if (s->keypads_found) snprintf(text, sizeof(text), "Eyes %s  WLED %s  ch %d  keys %d/%d", s->eyes_fresh ? "ok" : "--",
                                   s->wled_fresh ? "ok" : "--", s->channel, s->keypads_found, s->keypads_total);
    else snprintf(text, sizeof(text), "Eyes %s  WLED %s  ch %d  no keypads", s->eyes_fresh ? "ok" : "--",
                  s->wled_fresh ? "ok" : "--", s->channel);
    lv_label_set_text(m.links, text);
    set_col(m.links, s->eyes_fresh && s->wled_fresh && s->keypads_found ? COL_TITLE : COL_WARN);
    lv_label_set_text(m.log, s->log_text);
}

// ------------------------------------------------------------------ task and API

static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void display_task(void *arg)
{
    int64_t next_update = 0;
    bool lit = false;
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
            if (s.detail != !lv_obj_has_flag(m.detail, LV_OBJ_FLAG_HIDDEN)) {   // DETAIL: the small-text page
                if (s.detail) {
                    lv_obj_add_flag(m.simple, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(m.detail, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(m.detail, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(m.simple, LV_OBJ_FLAG_HIDDEN);
                }
            }
            ui_update(&s);
            simple_update(&s);
            static int64_t event_until;
            if (new_event) {
                lv_label_set_text(m.event, ev);
                lv_label_set_text(m.s_event, ev);   // the simple page shows it for a while
                event_until = now + SIMPLE_EVENT_MS * 1000LL;
            } else if (event_until && now > event_until) {
                lv_label_set_text(m.s_event, "");
                event_until = 0;
            }
        }
        if (shot_wanted && !shot_filling) {
            shot_wanted = false;
            shot_filling = true;
            lv_obj_invalidate(lv_screen_active());
        }
        const uint32_t wait = lv_timer_handler();
        if (!lit) {   // the first frame is out: light the panel (no garbage at power-up)
            lit = true;
            param(0x51, (uint8_t[]){ BRIGHTNESS }, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(wait < 10 ? 10 : wait > UPDATE_MS ? UPDATE_MS : wait));
    }
}

esp_err_t display_start(display_action_cb_t on_action)
{
    (void)on_action;   // no touch on this board
    sent = xSemaphoreCreateBinary();
    ESP_RETURN_ON_ERROR(panel_init(), TAG, "panel init failed");
    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    const size_t bytes = LCD_W * BUF_ROWS * 2;
    uint8_t *b1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint8_t *b2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(b1 && b2, ESP_ERR_NO_MEM, TAG, "no memory for the draw buffers");
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_add_event_cb(disp, round_area, LV_EVENT_INVALIDATE_AREA, NULL);
    ui_build();
    ESP_LOGI(TAG, "1.91 AMOLED, %d x %d", LCD_W, LCD_H);
    return xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 2, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

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
    const size_t n = LCD_W * LCD_H * 2;
    if (!shot_ready) shot_ready = xSemaphoreCreateBinary();
    if (!shot_buf) shot_buf = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (!shot_ready || !shot_buf) return;
    xSemaphoreTake(shot_ready, 0);
    shot_wanted = true;
    if (xSemaphoreTake(shot_ready, pdMS_TO_TICKS(2000)) != pdTRUE) {
        shot_wanted = shot_filling = false;
        printf("SHOT failed\n");
        return;
    }
    if (rle) {
        static uint8_t *runs;
        if (!runs) runs = heap_caps_malloc(n / 2 * 3, MALLOC_CAP_SPIRAM);
        if (!runs) {
            printf("SHOT failed\n");
            return;
        }
        size_t k = 0;
        for (size_t i = 0; i < n;) {
            const uint8_t lo = shot_buf[i], hi = shot_buf[i + 1];
            size_t len = 1;
            while (len < 256 && i + len * 2 < n && shot_buf[i + len * 2] == lo && shot_buf[i + len * 2 + 1] == hi) len++;
            runs[k++] = (uint8_t)(len - 1);
            runs[k++] = lo;
            runs[k++] = hi;
            i += len * 2;
        }
        printf("SHOT %d %d rle565le\n", LCD_W, LCD_H);
        shot_print_b64(runs, k);
    } else {
        printf("SHOT %d %d rgb565le\n", LCD_W, LCD_H);
        shot_print_b64(shot_buf, n);
    }
    printf("SHOT END\n");
}

void display_show_page(int page) { page_wanted = page; }   // one page here

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

// On the Live and Standby monitors instead (the 1.64 boards).
void display_preview(bool standby, const void *payload, size_t len) { (void)standby; (void)payload; (void)len; }
void display_fin_key(bool down) { (void)down; }
void display_eye_look(const nl_eye_look_t *look) { (void)look; }
void display_eye_test(int preset, int state) { (void)preset; (void)state; }
void display_palettes(bool on, int first, uint32_t held) { (void)on; (void)first; (void)held; }
void display_survey(const nl_survey_t *results, bool busy) { (void)results; (void)busy; }
