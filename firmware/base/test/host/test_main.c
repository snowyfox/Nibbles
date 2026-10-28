// Host tests for the base station's keypad logic and power arithmetic.
//   make -C firmware/base/test/host
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls.h"
#include "nibbles_link.h"
#include "power_math.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static ctl_action_t acts[CTL_MAX_ACTIONS];
static int nacts;
static uint32_t now;

static int press(ctl_t *c, ctl_key_t k) { now += 20; return nacts = ctl_key(c, k, true, now, acts); }
static int release(ctl_t *c, ctl_key_t k) { now += 20; return nacts = ctl_key(c, k, false, now, acts); }
static int tap(ctl_t *c, ctl_key_t k) { press(c, k); return release(c, k); }

static bool has(ctl_act_type_t type, int target, int value)
{
    for (int i = 0; i < nacts; i++)
        if (acts[i].type == type && (target < 0 || acts[i].target == target) && (value < -1000 || acts[i].value == value))
            return true;
    return false;
}

static void live(ctl_t *c, int body, int eyes, int fins)
{
    const ctl_scene_t s = { body, eyes, fins };
    ctl_set_live(c, &s, 128, 1);
}

static void test_standby_fills_from_live(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    CHECK(!c.standby_set);
    live(&c, 3, 0, -1);           // WLED heard first
    CHECK(c.standby.body == 3 && c.standby.eyes == 0 && !c.standby_set);
    live(&c, 3, 5, 0);            // then the eyes (the fins follow the body)
    CHECK(c.standby.body == 3 && c.standby.eyes == 5 && c.standby.fins == 0 && c.standby_set);
    CHECK(ctl_standby_diff(&c) == 0);
    live(&c, 4, 5, 0);            // Live moving on by itself doesn't drag Standby along
    CHECK(c.standby.body == 3);
}

static void test_number_is_a_bump(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    // ALL: holding 4 shows preset 4 on both while held, and nothing else changes
    press(&c, KEY_NUM4);
    CHECK(has(ACT_BUMP, TGT_BODY, 4) && has(ACT_BUMP, TGT_EYES, 4));
    CHECK(acts[0].bump == NL_BUMP_PRESET);
    CHECK(!has(ACT_PRESET, -1, -10000));
    release(&c, KEY_NUM4);
    CHECK(has(ACT_BUMP_END, TGT_BODY, -10000) && has(ACT_BUMP_END, TGT_EYES, -10000));
    CHECK(c.live.body == 1 && c.standby.body == 1);
}

static void test_latch_number(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_NUM5);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 5) && has(ACT_PRESET, TGT_EYES, 5));
    CHECK(acts[0].type == ACT_PRESET && acts[nacts - 1].type == ACT_BUMP_END);  // commands before the bumps let go
    CHECK(acts[0].target == TGT_BODY && acts[0].bump == 1);   // ALL: the WLED preset's eye settings may come too
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_NUM5) == 0);          // nothing changes back
    CHECK(c.live.body == 5 && c.live.eyes == 5);
    CHECK(c.standby.body == 1 && c.standby.eyes == 1);   // the old Live, one TAKE away
}

static void test_take_is_a_bump_latch_swaps(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_NUM3);                        // 3 Live, 1 in Standby
    press(&c, KEY_LATCH);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM3);
    // TAKE alone: Standby while held, back after
    press(&c, KEY_TAKE);
    CHECK(has(ACT_BUMP, TGT_BODY, 1) && has(ACT_BUMP, TGT_EYES, 1));
    release(&c, KEY_TAKE);
    CHECK(!has(ACT_PRESET, -1, -10000) && has(ACT_BUMP_END, TGT_BODY, -10000));
    CHECK(c.live.body == 3 && c.standby.body == 1);
    // TAKE + LATCH: the swap sticks
    press(&c, KEY_TAKE);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 1) && has(ACT_PRESET, TGT_EYES, 1));
    CHECK(acts[nacts - 1].type == ACT_BUMP_END);
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_TAKE) == 0);
    CHECK(c.live.body == 1 && c.standby.body == 3);
}

static void test_take_with_nothing_staged(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    CHECK(press(&c, KEY_TAKE) == 0);
    CHECK(release(&c, KEY_TAKE) == 0);
    CHECK(press(&c, KEY_LATCH) == 0);           // LATCH with nothing held: nothing
    release(&c, KEY_LATCH);
}

static void test_latch_first_cues_standby(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_LATCH);
    CHECK(tap(&c, KEY_NUM7) == 0 && nacts == 0);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 7 && c.standby.eyes == 7 && c.live.body == 1);
    // LATCH + TAKE: Standby back to Live
    press(&c, KEY_LATCH);
    tap(&c, KEY_TAKE);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 1 && ctl_standby_diff(&c) == 0);
}

static void test_targets(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    tap(&c, KEY_EYES);                          // tap latches the target
    CHECK(c.target == TGT_EYES);
    press(&c, KEY_NUM6);
    CHECK(nacts == 1 && acts[0].target == TGT_EYES && acts[0].value == 6);
    tap(&c, KEY_LATCH);
    release(&c, KEY_NUM6);
    CHECK(c.live.eyes == 6 && c.live.body == 1);
    // hold BODY + a number: momentary target, back to EYES after
    press(&c, KEY_BODY);
    press(&c, KEY_NUM3);
    CHECK(nacts == 1 && acts[0].target == TGT_BODY);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 3) && acts[0].bump == 0);   // BODY: body only
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM3);
    release(&c, KEY_BODY);
    CHECK(c.target == TGT_EYES && c.live.body == 3);
    // FINS: a number bumps that fin preset; LATCH keeps it
    tap(&c, KEY_FINS);
    press(&c, KEY_NUM2);
    CHECK(nacts == 1 && acts[0].type == ACT_BUMP && acts[0].target == TGT_FINS && acts[0].value == 2);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 2) && c.live.fins == 2 && c.standby.fins == 0);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM2);
    CHECK(c.live.body == 3);                    // the body kept its preset
}

static void test_pages_and_brightness(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    tap(&c, KEY_PAGE);
    CHECK(c.page == 1);
    press(&c, KEY_NUM1);
    CHECK(has(ACT_BUMP, TGT_BODY, 9));
    release(&c, KEY_NUM1);
    // PAGE held + number: brightness, and the page doesn't flip
    press(&c, KEY_PAGE);
    press(&c, KEY_NUM8);
    CHECK(has(ACT_BRIGHTNESS, -1, 255));
    release(&c, KEY_NUM8);
    press(&c, KEY_NUM4);
    CHECK(has(ACT_BRIGHTNESS, -1, 127));
    release(&c, KEY_NUM4);
    release(&c, KEY_PAGE);
    CHECK(c.page == 1);
    tap(&c, KEY_PAGE);
    CHECK(c.page == 0);
}

static void test_fins_and_all(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 4);                          // the fins on their own preset 4
    // ALL + a number: the whole shark, so the fins show the body's preset while held...
    press(&c, KEY_NUM6);
    CHECK(has(ACT_BUMP, TGT_BODY, 6) && has(ACT_BUMP, TGT_EYES, 6) && has(ACT_BUMP, TGT_FINS, 0));
    // ...and LATCH puts them back to following the body
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 0) && c.live.fins == 0 && c.standby.fins == 4);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM6);
    // TAKE swaps the fins too
    press(&c, KEY_TAKE);
    CHECK(has(ACT_BUMP, TGT_FINS, 4));
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 4) && c.live.fins == 4);
    release(&c, KEY_LATCH);
    release(&c, KEY_TAKE);
    // FIN SENSOR: the angle sensor, apart from the presets
    tap(&c, KEY_FIN_SENSOR);
    press(&c, KEY_FIN_SENSOR);
    CHECK(has(ACT_FINS, TGT_FINS, 1) || has(ACT_FINS, TGT_FINS, 0));
    release(&c, KEY_FIN_SENSOR);
    CHECK(c.live.fins == 4);
}

static void test_flash_blackout(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_FLASH);
    CHECK(has(ACT_BUMP, TGT_BODY, 0) && has(ACT_BUMP, TGT_EYES, 0) && acts[0].bump == NL_BUMP_FLASH);
    release(&c, KEY_FLASH);
    CHECK(nacts == 2 && acts[0].type == ACT_BUMP_END);
    tap(&c, KEY_EYES);
    press(&c, KEY_BLACKOUT);
    CHECK(nacts == 1 && acts[0].target == TGT_EYES && acts[0].bump == NL_BUMP_BLACKOUT);
    release(&c, KEY_BLACKOUT);
}

static void test_lock(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    ctl_tick(&c, now, acts);
    CHECK(c.locked);
    now += 500;
    ctl_tick(&c, now, acts);                    // still held: no second toggle
    CHECK(c.locked);
    release(&c, KEY_UNLOCK);
    CHECK(tap(&c, KEY_NUM3) == 0 && c.standby.body == 1);
    CHECK(press(&c, KEY_FLASH) == 0);
    release(&c, KEY_FLASH);
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_NUM1] == 0 && rgb[KEY_UNLOCK] != 0);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    ctl_tick(&c, now, acts);
    CHECK(!c.locked);
    release(&c, KEY_UNLOCK);
    // locking while a bump is held releases it
    press(&c, KEY_FLASH);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    nacts = ctl_tick(&c, now, acts);
    CHECK(c.locked && has(ACT_BUMP_END, TGT_BODY, -10000));
}

static void test_autolock(void)
{
    ctl_t c;
    ctl_init(&c, 60000);
    live(&c, 1, 1, 0);
    tap(&c, KEY_NUM2);
    now += 59000;
    ctl_tick(&c, now, acts);
    CHECK(!c.locked);
    now += 2000;
    ctl_tick(&c, now, acts);
    CHECK(c.locked);
}

static void test_leds(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_LATCH);
    tap(&c, KEY_NUM3);
    release(&c, KEY_LATCH);
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, 0, rgb);
    CHECK(rgb[KEY_NUM1] == 0xff1010);           // live
    CHECK(rgb[KEY_NUM3] == 0x00e040);           // staged
    CHECK(rgb[KEY_ALL] == 0xffffff);            // selected target
    CHECK(rgb[KEY_TAKE] != 0x202020);           // something to take
}

static void test_names(void)
{
    for (int k = 1; k < KEY_COUNT; k++) CHECK((int)ctl_key_by_name(ctl_key_name((ctl_key_t)k)) == k);
    CHECK(ctl_key_by_name("nope") == KEY_NONE);
}

static void test_power_math(void)
{
    CHECK(pack_percent(16800, 0) == 100);
    CHECK(pack_percent(12000, 0) == 0);
    const int mid = pack_percent(15200, 0);    // 3.8 V per cell
    CHECK(mid == 55);
    CHECK(pack_percent(15200, 4000) > mid);     // sagging under load reads higher once corrected
    CHECK(runtime_minutes(50, 40000) == 60);    // 40 Wh at 40 W
    CHECK(runtime_minutes(50, 100) == -1);
    CHECK(ina226_calibration() == 2560);
    CHECK(ina226_bus_mv(13440) == 16800);
    CHECK(ina226_current_ma(0xFFFF) == -1);
    CHECK(ina226_current_ma(7000) == 7000);
    CHECK(ina226_power_mw(4) == 100);
    CHECK(bq_charge_state(0x60) == CHG_FAST);
    CHECK(bq_charge_state(0xE0) == CHG_DONE);
    CHECK(bq_die_tenths_c(0xFFFE) == -10);      // -1 C
    CHECK(bq_die_tenths_c(70) == 350);
    // At 25 C the NTC is 10k: 10k || 30.1k = 7.506k, TS = 7.506 / (5.23 + 7.506) = 58.9%
    const int t25 = ts_cell_tenths_c(589);
    CHECK(t25 > 240 && t25 < 260);
    CHECK(ts_cell_tenths_c(560) > t25);         // lower TS = hotter (NTC)
    CHECK(ts_cell_tenths_c(0) == INT16_MIN);
}

int main(void)
{
    test_standby_fills_from_live();
    test_number_is_a_bump();
    test_latch_number();
    test_take_is_a_bump_latch_swaps();
    test_take_with_nothing_staged();
    test_latch_first_cues_standby();
    test_targets();
    test_pages_and_brightness();
    test_fins_and_all();
    test_flash_blackout();
    test_lock();
    test_autolock();
    test_leds();
    test_names();
    test_power_math();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all base host tests passed\n");
    return 0;
}
