// Preview screen: a Waveshare ESP32-S3-AMOLED-1.91 (RM67162, 536 x 240 on QSPI)
// showing a side view of the shark from a brain's preview stream
// (NL_MSG_PREVIEW). With no stream it shows a test pattern, so the screen and
// the LED map can be checked with no brain running.
#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_pos.h"
#include "nibbles_espnow.h"
#include "nibbles_link.h"
#include "nvs_flash.h"

static const char *TAG = "preview";

#define PIN_CS      6
#define PIN_SCLK    47
#define PIN_D0      18
#define PIN_D1      7
#define PIN_D2      48
#define PIN_D3      5
#define PIN_RST     17
#define LCD_W       536
#define LCD_H       240
#define CHUNK_ROWS  24
#define BRIGHTNESS  0xB0
#define FRAME_MS    40      // at most 25 fps on screen
#define STREAM_LOST_MS 2000
#define START_CHANNEL 11

#define CMD(c)        ((0x02u << 24) | ((uint32_t)(c) << 8))     // command and parameters on one line
#define COLOR_CMD(c)  ((0x32u << 24) | ((uint32_t)(c) << 8))     // pixels on four

static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t sent;
static uint16_t *fb, *dma;

// The stream, written by the radio task.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t incoming[LED_POS_COUNT];
static volatile int64_t last_rx_us;
static volatile uint32_t rx_frames, rx_chunks, rx_lost;
static int last_frame = -1;

static bool on_sent(esp_lcd_panel_io_handle_t h, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sent, &woken);
    return woken == pdTRUE;
}

static void param(uint8_t c, const void *data, size_t n) { ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, CMD(c), data, n)); }

static void lcd_init(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCLK, .data0_io_num = PIN_D0, .data1_io_num = PIN_D1,
        .data2_io_num = PIN_D2, .data3_io_num = PIN_D3,
        .max_transfer_sz = LCD_W * CHUNK_ROWS * 2 + 16,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t cfg = {
        .cs_gpio_num = PIN_CS, .dc_gpio_num = -1, .spi_mode = 0, .pclk_hz = 40000000,
        .trans_queue_depth = 4, .on_color_trans_done = on_sent,
        .lcd_cmd_bits = 32, .lcd_param_bits = 8, .flags.quad_mode = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &cfg, &io));

    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    param(0x11, NULL, 0);                          // sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    param(0x3A, (uint8_t[]){ 0x55 }, 1);           // 16 bits a pixel
    param(0x36, (uint8_t[]){ 0xA0 }, 1);           // landscape, the way up the USB port makes natural here
    param(0x2A, (uint8_t[]){ 0, 0, (LCD_W - 1) >> 8, (LCD_W - 1) & 255 }, 4);
    param(0x2B, (uint8_t[]){ 0, 0, (LCD_H - 1) >> 8, (LCD_H - 1) & 255 }, 4);
    param(0x51, (uint8_t[]){ 0 }, 1);
    param(0x29, NULL, 0);                          // display on
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void lcd_show(void)
{
    for (int y = 0; y < LCD_H; y += CHUNK_ROWS) {
        const int rows = LCD_H - y < CHUNK_ROWS ? LCD_H - y : CHUNK_ROWS;
        memcpy(dma, fb + y * LCD_W, rows * LCD_W * 2);
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, COLOR_CMD(y ? 0x3C : 0x2C), dma, rows * LCD_W * 2));
        xSemaphoreTake(sent, portMAX_DELAY);
    }
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    const uint16_t c = (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3;
    return c >> 8 | c << 8;   // the panel takes the high byte first
}

// A dot about 5 px across: the LEDs are 3-8 px apart here, so they read as a shark.
static inline void dot(int x, int y, uint16_t c)
{
    static const int8_t off[][2] = { {0,-2},{-1,-1},{0,-1},{1,-1},{-2,0},{-1,0},{0,0},{1,0},{2,0},{-1,1},{0,1},{1,1},{0,2} };
    for (size_t i = 0; i < sizeof(off) / sizeof(off[0]); i++) {
        const int px = x + off[i][0], py = y + off[i][1];
        if (px >= 0 && px < LCD_W && py >= 0 && py < LCD_H) fb[py * LCD_W + px] = c;
    }
}

static void on_radio(const uint8_t mac[6], const nl_radio_hdr_t *hdr, const uint8_t *payload, size_t len)
{
    if (hdr->type != NL_MSG_PREVIEW || len <= NL_PREVIEW_HEADER_LEN) return;
    const nl_preview_t *m = (const nl_preview_t *)payload;
    size_t n = len - NL_PREVIEW_HEADER_LEN;
    if (m->start >= LED_POS_COUNT) return;
    if (m->start + n > LED_POS_COUNT) n = LED_POS_COUNT - m->start;
    taskENTER_CRITICAL(&lock);
    memcpy(incoming + m->start, m->rgb332, n);
    taskEXIT_CRITICAL(&lock);
    rx_chunks++;
    last_rx_us = esp_timer_get_time();
    if (m->flags & NL_PREVIEW_LAST) {
        if (last_frame >= 0) rx_lost += (uint8_t)(m->frame - last_frame - 1);
        last_frame = m->frame;
        rx_frames++;
    }
}

// No stream: colour bands sweep along the shark, nose to tail.
static void test_pattern(uint8_t *out, float t)
{
    for (int i = 0; i < LED_POS_COUNT; i++) {
        const float a = led_pos[i][0] * 0.02f - t * 2.0f;
        const int r = (sinf(a) + 1) * 3.5f, g = (sinf(a + 2.09f) + 1) * 3.5f, b = (sinf(a + 4.19f) + 1) * 1.5f;
        out[i] = r << 5 | g << 2 | b;
    }
}

static void draw_task(void *arg)
{
    static uint8_t now[LED_POS_COUNT];
    static uint16_t lut[256];
    for (int i = 0; i < 256; i++) lut[i] = rgb565((i >> 5) * 255 / 7, ((i >> 2) & 7) * 255 / 7, (i & 3) * 255 / 3);
    int64_t last_log = 0;
    uint32_t shown = 0;
    bool lit = false;
    for (;;) {
        const int64_t t = esp_timer_get_time();
        const bool stream = last_rx_us && t - last_rx_us < STREAM_LOST_MS * 1000LL;
        if (stream) {
            taskENTER_CRITICAL(&lock);
            memcpy(now, incoming, sizeof(now));
            taskEXIT_CRITICAL(&lock);
        } else {
            test_pattern(now, t / 1e6f);
        }
        memset(fb, 0, LCD_W * LCD_H * 2);
        for (int i = 0; i < LED_POS_COUNT; i++) dot(led_pos[i][0], led_pos[i][1], lut[now[i]]);
        lcd_show();
        if (!lit) {     // after the first frame, so the panel's old contents never show
            param(0x51, (uint8_t[]){ BRIGHTNESS }, 1);
            lit = true;
        }
        shown++;
        if (t - last_log >= 5000000) {
            ESP_LOGI(TAG, "%s; screen %.1f fps; rx %lu frames, %lu chunks, %lu frames lost; %u KB free",
                     stream ? "stream" : "NO STREAM (test pattern)", shown * 1e6f / (t - last_log + 1), (unsigned long)rx_frames,
                     (unsigned long)rx_chunks, (unsigned long)rx_lost, (unsigned)(esp_get_free_heap_size() / 1024));
            shown = 0;
            last_log = t;
        }
        const int64_t spent_ms = (esp_timer_get_time() - t) / 1000;
        vTaskDelay(pdMS_TO_TICKS(spent_ms < FRAME_MS ? FRAME_MS - spent_ms : 1));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    sent = xSemaphoreCreateBinary();
    fb = heap_caps_malloc(LCD_W * LCD_H * 2, MALLOC_CAP_SPIRAM);
    dma = heap_caps_malloc(LCD_W * CHUNK_ROWS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(fb && dma);
    lcd_init();
    const nl_espnow_config_t radio = {
        .role = NL_ROLE_PREVIEW, .side = NL_SIDE_NONE, .fw = 1, .anchor = false,
        .anchor_channel = START_CHANNEL, .anchor_fallback_ms = 0,   // only follows WLED's channel
        .handler = on_radio, .core = 0,
    };
    ESP_ERROR_CHECK(nl_espnow_start(&radio));
    xTaskCreatePinnedToCore(draw_task, "draw", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "up: %d LEDs on %d x %d", LED_POS_COUNT, LCD_W, LCD_H);
}
