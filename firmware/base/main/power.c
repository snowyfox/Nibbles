#include "power.h"

#include <math.h>
#include <string.h>
#include "config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_math.h"

static const char *TAG = "power";

#define BQ_ADDR       0x6B
#define INA_ADDR      0x40
#define EXP_ADDR      0x20
#define EXP_LIGHT_OFF 0x01
#define EXP_PACK_OFF  0x02
#define EXP_EFUSE_PG  0x08

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t bq, ina, exp_dev;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static power_status_t st;
static volatile bool want_lights = true, want_pack = true, sim;

i2c_master_bus_handle_t power_bus(void) { return bus; }
void power_sim(bool on) { sim = on; }
void power_set_lights(bool on) { want_lights = on; }
void power_set_pack(bool on) { want_pack = on; }

void power_get(power_status_t *out)
{
    taskENTER_CRITICAL(&lock);
    *out = st;
    taskEXIT_CRITICAL(&lock);
}

// ------------------------------------------------------------------ I2C helpers
static i2c_master_dev_handle_t add_dev(uint8_t addr)
{
    i2c_master_dev_handle_t d = NULL;
    const i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr,
                                      .scl_speed_hz = POWER_I2C_HZ };
    i2c_master_bus_add_device(bus, &cfg, &d);
    return d;
}

static esp_err_t rd(i2c_master_dev_handle_t d, uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(d, &reg, 1, buf, n, 20);
}

static esp_err_t rd16(i2c_master_dev_handle_t d, uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    ESP_RETURN_ON_ERROR(rd(d, reg, b, 2), TAG, "read");
    *v = b[0] << 8 | b[1];   // both chips send the high byte first
    return ESP_OK;
}

static esp_err_t wr8(i2c_master_dev_handle_t d, uint8_t reg, uint8_t v)
{
    const uint8_t b[2] = { reg, v };
    return i2c_master_transmit(d, b, 2, 20);
}

static esp_err_t wr16(i2c_master_dev_handle_t d, uint8_t reg, uint16_t v)
{
    const uint8_t b[3] = { reg, v >> 8, v & 0xff };
    return i2c_master_transmit(d, b, 3, 20);
}

static bool present(uint8_t addr) { return i2c_master_probe(bus, addr, 20) == ESP_OK; }

// ------------------------------------------------------------------ charger
// Settings (datasheet SLUSDV2C, section 7.5): the watchdog off (it would put
// ICHG and EN_CHG back to their defaults), D+/D- detection off (no USB on
// VBUS), the charge voltage and current, optionally the input current limit
// above the ILIM_HIZ pin's 2 A, and the ADC on. Read back what matters; if
// the charge voltage doesn't read back right, charging is turned off.
static bool bq_configure(void)
{
    uint8_t r;
    bool ok = true;
    ok &= rd(bq, 0x10, &r, 1) == ESP_OK && wr8(bq, 0x10, r & ~0x07) == ESP_OK;         // WATCHDOG = 0 (off)
    ok &= rd(bq, 0x11, &r, 1) == ESP_OK && wr8(bq, 0x11, r & ~0x40) == ESP_OK;         // AUTO_INDET_EN = 0
    ok &= wr16(bq, 0x01, CHG_VREG_MV / 10) == ESP_OK;                                  // VREG, 10 mV
    ok &= wr16(bq, 0x03, CHG_ICHG_MA / 10) == ESP_OK;                                  // ICHG, 10 mA
    if (CHG_IINDPM_MA) {
        ok &= wr16(bq, 0x06, CHG_IINDPM_MA / 10) == ESP_OK;                            // IINDPM, 10 mA
        ok &= rd(bq, 0x14, &r, 1) == ESP_OK && wr8(bq, 0x14, r & ~0x02) == ESP_OK;     // EN_EXTILIM = 0
    }
    ok &= wr8(bq, 0x2E, 0x80 | 1 << 4) == ESP_OK;  // ADC on, continuous, 14 bit
    uint16_t vreg = 0, ichg = 0;
    ok &= rd16(bq, 0x01, &vreg) == ESP_OK && rd16(bq, 0x03, &ichg) == ESP_OK;
    if (!ok || (vreg & 0x7FF) * 10 != CHG_VREG_MV || (ichg & 0x1FF) * 10 != CHG_ICHG_MA) {
        ESP_LOGE(TAG, "charger settings didn't stick (VREG %d mV, ICHG %d mA): charging off", (vreg & 0x7FF) * 10,
                 (ichg & 0x1FF) * 10);
        if (rd(bq, 0x0F, &r, 1) == ESP_OK) wr8(bq, 0x0F, r & ~0x20);                   // EN_CHG = 0
        return false;
    }
    ESP_LOGI(TAG, "charger set: %d mV, %d mA%s", CHG_VREG_MV, CHG_ICHG_MA,
             CHG_IINDPM_MA ? ", input limit raised" : ", input limit from ILIM_HIZ (2 A)");
    return true;
}

static bool bq_read(power_status_t *s)
{
    uint8_t stat[7], adc[18];
    if (rd(bq, 0x1B, stat, sizeof(stat)) != ESP_OK || rd(bq, 0x31, adc, sizeof(adc)) != ESP_OK) return false;
    s->chg_state = bq_charge_state(stat[1]);
    s->ts_flags = stat[4] & 0x0F;
    s->fault0 = stat[5];
    s->fault1 = stat[6];
    const bool vbus_present = stat[0] & 0x01;
#define A16(off) ((uint16_t)(adc[(off) - 0x31] << 8 | adc[(off) - 0x31 + 1]))
    s->ibus_ma = (int16_t)A16(0x31);
    s->ibat_ma = (int16_t)A16(0x33);
    s->vbus_mv = vbus_present ? A16(0x35) : 0;
    s->vbat_mv = A16(0x3B);
    s->vsys_mv = A16(0x3D);
    s->cell_c10 = ts_cell_tenths_c(bq_ts_tenths_pct(A16(0x3F)));
    s->die_c10 = bq_die_tenths_c(A16(0x41));
#undef A16
    s->docked |= vbus_present;
    return true;
}

// ------------------------------------------------------------------ INA226, expander
static bool ina_configure(void)
{
    uint16_t man = 0, die = 0;
    if (rd16(ina, 0xFE, &man) != ESP_OK || rd16(ina, 0xFF, &die) != ESP_OK) return false;
    if (man != 0x5449 || (die >> 4) != 0x226) {
        ESP_LOGE(TAG, "0x40 isn't an INA226 (maker %04x, die %04x)", man, die);
        return false;
    }
    // 16 averages, 1.1 ms conversions (about 35 ms per reading), shunt and bus continuous
    return wr16(ina, 0x00, 0x4000 | 2 << 9 | 4 << 6 | 4 << 3 | 7) == ESP_OK &&
           wr16(ina, 0x05, ina226_calibration()) == ESP_OK;
}

static bool ina_read(power_status_t *s)
{
    uint16_t v, i, p;
    if (rd16(ina, 0x02, &v) != ESP_OK || rd16(ina, 0x04, &i) != ESP_OK || rd16(ina, 0x03, &p) != ESP_OK) return false;
    s->light_mv = ina226_bus_mv(v);
    s->light_ma = ina226_current_ma(i);
    s->light_mw = ina226_power_mw(p);
    return true;
}

static bool exp_write(bool lights_off, bool pack_off)
{
    // Output levels first, then make P0 and P1 outputs (the rest stay inputs).
    const uint8_t out = (lights_off ? EXP_LIGHT_OFF : 0) | (pack_off ? EXP_PACK_OFF : 0);
    return wr8(exp_dev, 0x01, out) == ESP_OK && wr8(exp_dev, 0x03, (uint8_t)~(EXP_LIGHT_OFF | EXP_PACK_OFF)) == ESP_OK;
}

// ------------------------------------------------------------------ simulation
static void simulate(power_status_t *s, int64_t now_us)
{
    const double t = now_us / 1e6;
    s->sim = true;
    s->have_charger = s->have_monitor = s->have_expander = true;
    s->docked = fmod(t, 120) < 60;                  // a minute on the dock, a minute off
    s->charger_ok = true;
    s->vbat_mv = 15000 + (int)(400 * sin(t / 30));
    s->vbus_mv = !s->docked ? 0 : fmod(t, 240) < 120 ? 15600 : 20100;   // an external battery, then a USB-C supply
    const bool starved = s->docked && fmod(t, 240) >= 30 && fmod(t, 240) < 60;   // half a minute of a starved pack
    s->ibat_ma = starved ? 30 : s->docked ? 1850 : 0;   // the charger's current only (the lights bypass it)
    s->ibus_ma = starved ? 60 : s->docked ? 2000 : 0;
    s->vsys_mv = s->vbat_mv + 200;
    s->chg_state = s->docked ? CHG_FAST : CHG_NOT_CHARGING;
    s->die_c10 = 750 + (int)(350 * sin(t / 25));   // sweeps through the screen's warn and bad levels
    s->cell_c10 = 400 + (int)(180 * sin(t / 20));
    s->light_mv = s->docked ? 16400 : s->vbat_mv - 150;
    s->light_ma = 2500 + (int)(1500 * sin(t * 1.3));
    s->light_mw = s->light_mv * s->light_ma / 1000;
    s->efuse_pg = true;
    s->fault0 = s->fault1 = s->ts_flags = 0;
}

// ------------------------------------------------------------------ task
static void power_task(void *arg)
{
    bool bq_ok = false, ina_ok = false, exp_ok = false, cfg_ok = false;
    int64_t next_probe = 0;
    bool lights_off = false, pack_off = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
        const int64_t now = esp_timer_get_time();
        power_status_t s = { 0 };
        s.die_c10 = s.cell_c10 = INT16_MIN;
        if (sim) {
            simulate(&s, now);
        } else {
            if (now >= next_probe) {   // look for anything missing every 2 s
                next_probe = now + 2000000;
                if (!bq_ok && present(BQ_ADDR)) {
                    bq_ok = true;
                    cfg_ok = bq_configure();
                    ESP_LOGI(TAG, "charger found");
                }
                if (!ina_ok && present(INA_ADDR)) ina_ok = ina_configure();
                if (!exp_ok && present(EXP_ADDR)) {
                    exp_ok = exp_write(lights_off, pack_off);
                    if (exp_ok) ESP_LOGI(TAG, "expander found");
                }
            }
            s.docked = gpio_get_level(PIN_DOCK_DET);
            if (bq_ok && !(bq_ok = bq_read(&s))) ESP_LOGW(TAG, "charger lost");
            if (ina_ok && !(ina_ok = ina_read(&s))) ESP_LOGW(TAG, "current monitor lost");
            // The pack is only ever switched off while the dock can run the lights.
            const bool want_lights_off = !want_lights, want_pack_off = !want_pack && s.docked;
            if (exp_ok) {
                uint8_t in;
                if (want_lights_off != lights_off || want_pack_off != pack_off) {
                    exp_ok = exp_write(want_lights_off, want_pack_off);
                    if (exp_ok) {
                        lights_off = want_lights_off;
                        pack_off = want_pack_off;
                        ESP_LOGI(TAG, "lights %s, pack %s", lights_off ? "OFF" : "on", pack_off ? "OFF" : "on");
                    }
                }
                if (exp_ok && (exp_ok = rd(exp_dev, 0x00, &in, 1) == ESP_OK)) s.efuse_pg = in & EXP_EFUSE_PG;
            }
            s.have_charger = bq_ok;
            s.charger_ok = bq_ok && cfg_ok;
            s.have_monitor = ina_ok;
            s.have_expander = exp_ok;
            s.lights_off = lights_off;
            s.pack_off = pack_off;
        }
        // The pack: the charger's battery reading if it's there; otherwise the
        // lights' input when not docked (then it is the pack, less the diode).
        s.pack_mv = s.have_charger ? s.vbat_mv : (!s.docked && s.have_monitor) ? s.light_mv : 0;
        // The LEMO input and how the currents divide (power_math.h, power_flow).
        s.input = s.have_charger ? input_kind(s.vbus_mv) : INPUT_NONE;
        s.input_pct = s.input == INPUT_BATTERY ? pack_percent(s.vbus_mv, s.ibus_ma) : -1;
        if (s.have_charger) {
            const power_flow_t f = power_flow((input_kind_t)s.input, s.vbus_mv, s.ibus_ma, s.pack_mv, s.ibat_ma,
                                              s.have_monitor ? s.light_ma : 0, BASE_LOAD_MW, s.chg_state == CHG_DONE);
            s.in_ma = f.in_ma;
            s.in_split = f.split_permille;
            s.pack_ma = f.pack_ma;
            s.starved = f.starved;
        } else {
            s.in_ma = 0;
            s.in_split = -1;
            s.pack_ma = s.have_monitor && !s.docked ? -s.light_ma : 0;
            s.starved = false;
        }
        s.pack_pct = s.pack_mv ? pack_percent(s.pack_mv, -s.pack_ma) : -1;
        s.runtime_min = (!s.docked && s.pack_pct >= 0 && s.have_monitor) ? runtime_minutes(s.pack_pct, s.light_mw + BASE_LOAD_MW) : -1;

        taskENTER_CRITICAL(&lock);
        st = s;
        taskEXIT_CRITICAL(&lock);
    }
}

esp_err_t power_start(void)
{
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,          // the touch screen has I2C_NUM_0
        .sda_io_num = PIN_PWR_SDA,
        .scl_io_num = PIN_PWR_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,   // the board has 4.7k; these keep a bare header quiet
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &bus), TAG, "power i2c bus");
    bq = add_dev(BQ_ADDR);
    ina = add_dev(INA_ADDR);
    exp_dev = add_dev(EXP_ADDR);
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_DOCK_DET,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // the board pulls it down too (100k)
    };
    gpio_config(&io);
    return xTaskCreate(power_task, "power", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
