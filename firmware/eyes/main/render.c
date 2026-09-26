#include "render.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "bsp/display.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "board.h"
#include "config.h"
#include "presets.h"
#include "render_core.h"

static const char *TAG = "render";

static void *psram_alloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static uint16_t *bufs[STRIP_BUFFERS];
static int next_buf;
static SemaphoreHandle_t free_bufs;
static SemaphoreHandle_t helper_done;
static TaskHandle_t helper_task;
// Tearing avoidance. The panel refreshes from its own memory at ~59 Hz and
// pulses TE high for ~0.6 ms of blanking; scanning starts on the falling edge.
// Sending a frame takes longer than one refresh, so we start at a falling
// edge: the first refresh then stays ahead of our writes (showing the whole
// old frame) and the second finds them complete (the whole new frame), as
// long as the frame is sent within about two refreshes.
#define TE_GPIO          GPIO_NUM_13
#define TE_PERIOD_US     16900
#define TE_DEADLINE_US   (2 * TE_PERIOD_US - 700)
static SemaphoreHandle_t te_sem;
static volatile int64_t te_time_us, last_done_us;
static int64_t frame_te_us;
static int late_frames;
static int64_t worst_send_us, worst_prep_us;

// Work handed to the helper core for the current strip.
static uint16_t *job_buf;
static int job_y0, job_y1;

static void helper_main(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        render_core_rows(job_buf, job_y0, job_y1);
        xSemaphoreGive(helper_done);
    }
}

static void IRAM_ATTR on_te(void *arg)
{
    BaseType_t woken = pdFALSE;
    te_time_us = esp_timer_get_time();
    xSemaphoreGiveFromISR(te_sem, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    last_done_us = esp_timer_get_time();
    xSemaphoreGiveFromISR(free_bufs, &woken);
    return woken == pdTRUE;
}

esp_err_t render_init(void)
{
    const bsp_display_config_t cfg = {
        .max_transfer_sz = DISP_W * STRIP_ROWS * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&cfg, &panel, &panel_io), TAG, "display init failed");

    for (int i = 0; i < STRIP_BUFFERS; i++) {
        bufs[i] = heap_caps_malloc(DISP_W * STRIP_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_RETURN_ON_FALSE(bufs[i], ESP_ERR_NO_MEM, TAG, "no memory for strip buffer");
    }
    free_bufs = xSemaphoreCreateCounting(STRIP_BUFFERS, STRIP_BUFFERS);
    helper_done = xSemaphoreCreateBinary();
    te_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(te_sem, ESP_ERR_NO_MEM, TAG, "no memory for semaphores");
    const gpio_config_t te_cfg = {
        .pin_bit_mask = 1ULL << TE_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&te_cfg), TAG, "TE pin config failed");
    esp_err_t isr_err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(isr_err == ESP_OK || isr_err == ESP_ERR_INVALID_STATE, isr_err, TAG, "GPIO ISR service failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(TE_GPIO, on_te, NULL), TAG, "TE interrupt failed");
    ESP_RETURN_ON_FALSE(free_bufs && helper_done, ESP_ERR_NO_MEM, TAG, "no memory for semaphores");

    const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = on_color_done };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, NULL), TAG, "callback register failed");

    render_core_set_mirror(board_side() == NL_SIDE_PORT);  // the eyes' spirals mirror each other
    ESP_RETURN_ON_FALSE(render_core_init(psram_alloc), ESP_ERR_NO_MEM, TAG, "no memory for the render maps");
    BaseType_t ok = xTaskCreatePinnedToCore(helper_main, "render_helper", 3072, NULL, 7, &helper_task, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "helper task create failed");
    return ESP_OK;
}

void render_frame(const eye_params_t *p)
{
    const int64_t t0 = esp_timer_get_time();
    render_core_prepare(p);
    const int64_t prep_us = esp_timer_get_time() - t0;
    if (prep_us > worst_prep_us) worst_prep_us = prep_us;

    // Check the previous frame made its deadline, then wait for a fresh TE
    // falling edge. The timeout keeps things running if TE ever stops.
    if (frame_te_us) {
        const int64_t send_us = last_done_us - frame_te_us;
        if (send_us > TE_DEADLINE_US) late_frames++;
        if (send_us > worst_send_us) worst_send_us = send_us;
    }
    xSemaphoreTake(te_sem, 0);
    if (xSemaphoreTake(te_sem, pdMS_TO_TICKS(50)) == pdTRUE) frame_te_us = te_time_us;
    else frame_te_us = 0;

    for (int y = 0; y < DISP_H; y += STRIP_ROWS) {
        const int rows = DISP_H - y < STRIP_ROWS ? DISP_H - y : STRIP_ROWS;
        const int split = rows / 2;
        xSemaphoreTake(free_bufs, portMAX_DELAY);
        uint16_t *buf = bufs[next_buf];
        next_buf = (next_buf + 1) % STRIP_BUFFERS;

        // Helper core renders the bottom half of the strip while this core does the top.
        job_buf = buf + split * DISP_W;
        job_y0 = y + split;
        job_y1 = y + rows;
        xTaskNotifyGive(helper_task);
        render_core_rows(buf, y, y + split);
        xSemaphoreTake(helper_done, portMAX_DELAY);

        esp_lcd_panel_draw_bitmap(panel, 0, y, DISP_W, y + rows, buf);
    }
}

void render_take_stats(int *late, float *worst_ms, float *worst_prep_ms)
{
    *late = late_frames;
    *worst_ms = worst_send_us / 1000.0f;
    *worst_prep_ms = worst_prep_us / 1000.0f;
    late_frames = 0;
    worst_send_us = worst_prep_us = 0;
}

void render_set_preset(int index)
{
    render_core_set_preset(index);
}

void render_set_brightness(int percent)
{
    bsp_display_brightness_set(percent);
}
