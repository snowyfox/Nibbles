#include "rtc.h"

#include <sys/time.h>
#include <time.h>
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "log_format.h"

static const char *TAG = "rtc";

#define RTC_ADDR   0x51
#define REG_TIME   0x04    // seconds, minutes, hours, days, weekdays, months, years
#define HALF_US    5       // 100 kHz

static bool known, found;

static void line(int pin, int level) { gpio_set_level(pin, level); esp_rom_delay_us(HALF_US); }

static void i2c_start(void)
{
    line(PIN_RTC_SDA, 1); line(PIN_RTC_SCL, 1);
    line(PIN_RTC_SDA, 0); line(PIN_RTC_SCL, 0);
}

static void i2c_stop(void)
{
    line(PIN_RTC_SDA, 0); line(PIN_RTC_SCL, 1); line(PIN_RTC_SDA, 1);
}

static bool i2c_write(uint8_t b)   // true: acknowledged
{
    for (int i = 7; i >= 0; i--) {
        line(PIN_RTC_SDA, (b >> i) & 1);
        line(PIN_RTC_SCL, 1); line(PIN_RTC_SCL, 0);
    }
    line(PIN_RTC_SDA, 1);
    line(PIN_RTC_SCL, 1);
    const bool ack = gpio_get_level(PIN_RTC_SDA) == 0;
    line(PIN_RTC_SCL, 0);
    return ack;
}

static uint8_t i2c_read(bool more)
{
    uint8_t b = 0;
    line(PIN_RTC_SDA, 1);
    for (int i = 0; i < 8; i++) {
        line(PIN_RTC_SCL, 1);
        b = b << 1 | gpio_get_level(PIN_RTC_SDA);
        line(PIN_RTC_SCL, 0);
    }
    line(PIN_RTC_SDA, more ? 0 : 1);
    line(PIN_RTC_SCL, 1); line(PIN_RTC_SCL, 0);
    line(PIN_RTC_SDA, 1);
    return b;
}

static bool read_regs(uint8_t reg, uint8_t *out, int n)
{
    i2c_start();
    bool ok = i2c_write(RTC_ADDR << 1) && i2c_write(reg);
    if (ok) {
        i2c_start();
        ok = i2c_write(RTC_ADDR << 1 | 1);
        for (int i = 0; ok && i < n; i++) out[i] = i2c_read(i + 1 < n);
    }
    i2c_stop();
    return ok;
}

static bool write_regs(uint8_t reg, const uint8_t *in, int n)
{
    i2c_start();
    bool ok = i2c_write(RTC_ADDR << 1) && i2c_write(reg);
    for (int i = 0; ok && i < n; i++) ok = i2c_write(in[i]);
    i2c_stop();
    return ok;
}

static void set_system(uint32_t t)
{
    const struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

bool rtc_start(void)
{
    const gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_RTC_SDA | 1ULL << PIN_RTC_SCL, .mode = GPIO_MODE_INPUT_OUTPUT_OD,
                               .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&io);
    gpio_set_level(PIN_RTC_SDA, 1);
    gpio_set_level(PIN_RTC_SCL, 1);
    esp_rom_delay_us(50);
    uint8_t r[7];
    found = read_regs(REG_TIME, r, sizeof(r));
    if (!found) {
        ESP_LOGW(TAG, "no clock chip answering");
        return false;
    }
    const uint32_t t = rtc_regs_to_unix(r);
    known = t != 0;    // 0: the oscillator stopped (power lost) or nonsense
    if (known) {
        set_system(t);
        ESP_LOGI(TAG, "time from the clock chip: %lu", (unsigned long)t);
    } else {
        ESP_LOGW(TAG, "the clock chip lost its time; set it with \"time\" (tools/base_log.py time)");
    }
    return known;
}

bool rtc_chip_found(void) { return found; }
bool rtc_time_known(void) { return known; }
uint32_t rtc_now(void) { return known ? (uint32_t)time(NULL) : 0; }

bool rtc_set(uint32_t t)
{
    uint8_t r[7];
    rtc_unix_to_regs(t, r);
    if (!write_regs(REG_TIME, r, sizeof(r))) return false;
    set_system(t);
    known = true;
    return true;
}
