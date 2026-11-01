#include "logger.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log_format.h"
#include "power.h"
#include "power_math.h"
#include "sdmmc_cmd.h"

static const char *TAG = "logger";

#define MOUNT      "/sd"
#define DIR_PATH   MOUNT "/nibbles"
#define ROW_MS     1000
#define SYNC_S     10       // on the card at least this often, so a power cut loses little
#define RETRY_S    30
#define MIN_FREE_MB 200     // delete the oldest logs below this

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static logger_status_t st = { .enabled = true };
static int show_preset = -1, show_bri = -1, show_fps = -1, show_fins = -1;
static volatile bool want_enabled = true;
static volatile int want_tail;
static sdmmc_card_t *card;
static FILE *file;
static char path[40];

void logger_set_show(int wled_preset, int wled_bri, int wled_fps, int fin_follow)
{
    taskENTER_CRITICAL(&lock);
    show_preset = wled_preset; show_bri = wled_bri; show_fps = wled_fps; show_fins = fin_follow;
    taskEXIT_CRITICAL(&lock);
}

void logger_get(logger_status_t *out)
{
    taskENTER_CRITICAL(&lock);
    *out = st;
    taskEXIT_CRITICAL(&lock);
}

void logger_enable(bool on) { want_enabled = on; }
void logger_print_tail(int lines) { want_tail = lines; }

static bool mount_card(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount = { .format_if_mount_failed = false, .max_files = 2, .allocation_unit_size = 16 * 1024 };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = PIN_SD_CLK;
    slot.cmd = PIN_SD_CMD;
    slot.d0 = PIN_SD_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    const esp_err_t err = esp_vfs_fat_sdmmc_mount(MOUNT, &host, &slot, &mount, &card);
    if (err != ESP_OK) {
        card = NULL;
        ESP_LOGW(TAG, "no card (%s)", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "card: %s, %llu MB", card->cid.name, (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024));
    return true;
}

static void card_space(void)
{
    uint64_t total = 0, free_b = 0;
    if (esp_vfs_fat_info(MOUNT, &total, &free_b) != ESP_OK) return;
    taskENTER_CRITICAL(&lock);
    st.card_mb = total >> 20;
    st.free_mb = free_b >> 20;
    taskEXIT_CRITICAL(&lock);
}

// The highest and lowest log numbers on the card (-1 with none).
static void scan_logs(int *low, int *high)
{
    *low = *high = -1;
    DIR *d = opendir(DIR_PATH);
    if (!d) return;
    for (struct dirent *e; (e = readdir(d));) {
        const int n = log_file_number(e->d_name);
        if (n < 0) continue;
        if (*low < 0 || n < *low) *low = n;
        if (n > *high) *high = n;
    }
    closedir(d);
}

static void make_room(int keep)
{
    for (;;) {
        card_space();
        if (st.free_mb >= MIN_FREE_MB) return;
        int low, high;
        scan_logs(&low, &high);
        if (low < 0 || low == keep) return;
        char old[40];
        snprintf(old, sizeof(old), DIR_PATH "/L%05d.CSV", low);
        ESP_LOGW(TAG, "card nearly full: deleting %s", old);
        if (unlink(old) != 0) return;
    }
}

static bool open_log(void)
{
    mkdir(DIR_PATH, 0777);
    int low, high;
    scan_logs(&low, &high);
    const int n = high < 0 ? 1 : high + 1 > 99999 ? 1 : high + 1;
    snprintf(path, sizeof(path), DIR_PATH "/L%05d.CSV", n);
    file = fopen(path, "w");
    if (!file) {
        ESP_LOGW(TAG, "can't create %s", path);
        return false;
    }
    fputs(LOG_HEADER, file);
    taskENTER_CRITICAL(&lock);
    st.file_number = n;
    st.open = true;
    st.rows = 0;
    taskEXIT_CRITICAL(&lock);
    make_room(n);
    ESP_LOGI(TAG, "logging to %s", path);
    return true;
}

static void close_all(void)
{
    if (file) fclose(file);
    file = NULL;
    if (card) esp_vfs_fat_sdcard_unmount(MOUNT, card);
    card = NULL;
    taskENTER_CRITICAL(&lock);
    st.open = st.mounted = false;
    taskEXIT_CRITICAL(&lock);
}

static void sample(log_sample_t *s)
{
    power_status_t p;
    power_get(&p);
    *s = (log_sample_t){
        .t_s = (uint32_t)(esp_timer_get_time() / 1000000), .sim = p.sim,
        .pack_mv = p.pack_mv, .pack_pct = p.pack_pct, .pack_ma = p.pack_ma,
        .input = p.input == INPUT_SUPPLY ? "usb" : p.input == INPUT_BATTERY ? "battery" : "none",
        .vbus_mv = p.vbus_mv, .ibus_ma = p.ibus_ma, .in_ma = p.in_ma, .ibat_ma = p.ibat_ma, .vsys_mv = p.vsys_mv,
        .light_mv = p.light_mv, .light_ma = p.light_ma, .light_mw = p.light_mw, .cell_c10 = p.cell_c10, .die_c10 = p.die_c10,
        .chg_state = p.chg_state, .fault0 = p.fault0, .fault1 = p.fault1, .ts_flags = p.ts_flags,
        .efuse_pg = p.efuse_pg, .lights_off = p.lights_off, .pole_off = p.pole_off,
    };
    taskENTER_CRITICAL(&lock);
    s->wled_preset = show_preset; s->wled_bri = show_bri; s->wled_fps = show_fps; s->fin_follow = show_fins;
    s->dropped = st.dropped;
    taskEXIT_CRITICAL(&lock);
}

static void print_tail(int lines)
{
    if (!file) { printf("LOG no file open\n"); return; }
    fflush(file);
    FILE *f = fopen(path, "r");
    if (!f) { printf("LOG can't read %s\n", path); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    long from = size - (long)lines * 200;
    fseek(f, from > 0 ? from : 0, SEEK_SET);
    static char line[256];
    if (from > 0) fgets(line, sizeof(line), f);   // part of a line
    while (fgets(line, sizeof(line), f)) printf("LOG %s", line);
    fclose(f);
    printf("LOG END %s, %ld bytes\n", path, size);
}

static void logger_task(void *arg)
{
    static char row[320];
    int64_t next_row = esp_timer_get_time(), next_try = 0, next_sync = 0, next_space = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        const int64_t now = esp_timer_get_time();
        taskENTER_CRITICAL(&lock);
        st.enabled = want_enabled;
        taskEXIT_CRITICAL(&lock);
        if (!want_enabled) {
            if (card) { close_all(); ESP_LOGI(TAG, "off"); }
            continue;
        }
        if (!card && now >= next_try) {
            next_try = now + RETRY_S * 1000000LL;
            if (mount_card()) {
                taskENTER_CRITICAL(&lock);
                st.mounted = true;
                taskEXIT_CRITICAL(&lock);
                if (!open_log()) close_all();
            }
        }
        if (want_tail) {
            print_tail(want_tail);
            want_tail = 0;
        }
        if (now < next_row) continue;
        next_row += ROW_MS * 1000LL;
        if (next_row < now) next_row = now + ROW_MS * 1000LL;   // fell behind: don't catch up in a burst
        if (!file) {
            if (card) continue;
            taskENTER_CRITICAL(&lock);
            st.dropped++;
            taskEXIT_CRITICAL(&lock);
            continue;
        }
        log_sample_t s;
        sample(&s);
        const size_t n = log_row(row, sizeof(row), &s);
        const int64_t t0 = esp_timer_get_time();
        bool ok = n && fwrite(row, 1, n, file) == n;
        if (ok && now >= next_sync) {
            next_sync = now + SYNC_S * 1000000LL;
            ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
        }
        const uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        taskENTER_CRITICAL(&lock);
        if (ok) st.rows++;
        else st.dropped++;
        if (ms > st.max_write_ms) st.max_write_ms = ms;
        taskEXIT_CRITICAL(&lock);
        if (!ok) {
            ESP_LOGW(TAG, "write failed: the card is gone or full; retrying in %d s", RETRY_S);
            close_all();
            next_try = now + RETRY_S * 1000000LL;
        } else if (now >= next_space) {
            next_space = now + 60000000LL;
            make_room(st.file_number);
        }
    }
}

esp_err_t logger_start(void)
{
    return xTaskCreate(logger_task, "logger", 6144, NULL, 1, NULL) == pdPASS ? ESP_OK : ESP_FAIL;
}
