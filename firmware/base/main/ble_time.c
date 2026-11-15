#include "ble_time.h"

#include "sdkconfig.h"

#if !CONFIG_BT_NIMBLE_ENABLED
// Builds without Bluetooth (the 3.49, which has its clock chip).
void ble_time_start(void) {}
bool ble_time_poll(bool stop_now, uint32_t *unix_time) { (void)stop_now; (void)unix_time; return false; }
ble_time_state_t ble_time_state(void) { return BLE_TIME_OFF; }
#else

#include <string.h>
#include <time.h>
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_store_config_init(void);

static const char *TAG = "ble_time";

#define CTS_SVC        0x1805
#define CTS_CUR_TIME   0x2A2B   // year (LE16), month, day, hours, minutes, seconds, weekday, 1/256 s, reason
#define CTS_LOCAL_INFO 0x2A0F   // time zone (int8, 15 min units), DST offset (uint8, 15 min units; 255 unknown)

static volatile ble_time_state_t state;
static volatile bool running, finished;   // finished: read, or failed for good; the main loop switches off
static volatile uint32_t got_time;        // a reading for the main loop (0: none)
static int64_t started_us, finished_us;
static volatile bool read_ok;
static uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t svc_start, svc_end, cur_handle, info_handle;
static uint8_t cur[10];
static bool have_cur;

static void advertise(void);

// ------------------------------------------------------------------ the remote
// iOS lists in Settings > Bluetooth, and reconnects by itself, only a few
// kinds of accessory: HID among them (LightBlue worked but had to connect by
// hand after every power-up, 2026-11-14). So the base also looks like a media
// remote ("consumer control": HID over GATT with one 16-bit usage report) that
// never presses anything. Pair once from Settings; at each power-up the phone
// reconnects, its time is read, and Bluetooth goes off again.
static const uint8_t report_map[] = {
    0x05, 0x0C,        // usage page: consumer
    0x09, 0x01,        // usage: consumer control
    0xA1, 0x01,        // collection: application
    0x85, 0x01,        //   report id 1
    0x15, 0x00,        //   logical minimum 0
    0x26, 0xFF, 0x03,  //   logical maximum 0x3FF
    0x19, 0x00,        //   usage minimum 0
    0x2A, 0xFF, 0x03,  //   usage maximum 0x3FF
    0x75, 0x10,        //   report size 16
    0x95, 0x01,        //   report count 1
    0x81, 0x00,        //   input: data, array
    0xC0,              // end collection
};
static const uint8_t hid_info[] = { 0x11, 0x01, 0x00, 0x02 };   // HID 1.11, no country, normally connectable
static const uint8_t report_ref[] = { 0x01, 0x01 };             // report id 1, input
static const uint8_t pnp_id[] = { 0x02, 0x3A, 0x30, 0x01, 0x00, 0x01, 0x00 };   // USB IDs (Espressif's 0x303A), product 1, version 1
static uint8_t battery = 100;
static uint16_t report_handle;

static int chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const uint16_t u = ble_uuid_u16(ctxt->chr ? ctxt->chr->uuid : ctxt->dsc->uuid);
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) return 0;   // control point, protocol mode: nothing to do
    const void *v = NULL;
    size_t n = 0;
    static const uint8_t no_key[2] = { 0, 0 };
    static const uint8_t report_mode = 1;
    switch (u) {
    case 0x2A4A: v = hid_info; n = sizeof(hid_info); break;
    case 0x2A4B: v = report_map; n = sizeof(report_map); break;
    case 0x2A4D: v = no_key; n = sizeof(no_key); break;
    case 0x2908: v = report_ref; n = sizeof(report_ref); break;
    case 0x2A4E: v = &report_mode; n = 1; break;
    case 0x2A19: v = &battery; n = 1; break;
    case 0x2A50: v = pnp_id; n = sizeof(pnp_id); break;
    case 0x2A29: v = "Nibbles"; n = 7; break;
    default: return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctxt->om, v, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

#define ENC_R (BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC)
static const struct ble_gatt_svc_def services[] = {
    {   // HID
        .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x1812),
        .characteristics = (struct ble_gatt_chr_def[]){
            { .uuid = BLE_UUID16_DECLARE(0x2A4A), .access_cb = chr_access, .flags = ENC_R },   // HID information
            { .uuid = BLE_UUID16_DECLARE(0x2A4B), .access_cb = chr_access, .flags = ENC_R },   // report map
            { .uuid = BLE_UUID16_DECLARE(0x2A4C), .access_cb = chr_access,                      // control point
              .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC },
            { .uuid = BLE_UUID16_DECLARE(0x2A4E), .access_cb = chr_access,                      // protocol mode
              .flags = ENC_R | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC },
            { .uuid = BLE_UUID16_DECLARE(0x2A4D), .access_cb = chr_access, .val_handle = &report_handle,   // the input report
              .flags = ENC_R | BLE_GATT_CHR_F_NOTIFY,
              .descriptors = (struct ble_gatt_dsc_def[]){
                  { .uuid = BLE_UUID16_DECLARE(0x2908), .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC, .access_cb = chr_access },
                  { 0 } } },
            { 0 } },
    },
    {   // battery: HID over GATT asks for one
        .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x180F),
        .characteristics = (struct ble_gatt_chr_def[]){
            { .uuid = BLE_UUID16_DECLARE(0x2A19), .access_cb = chr_access, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
            { 0 } },
    },
    {   // device information
        .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics = (struct ble_gatt_chr_def[]){
            { .uuid = BLE_UUID16_DECLARE(0x2A50), .access_cb = chr_access, .flags = BLE_GATT_CHR_F_READ },   // PnP ID
            { .uuid = BLE_UUID16_DECLARE(0x2A29), .access_cb = chr_access, .flags = BLE_GATT_CHR_F_READ },   // maker
            { 0 } },
    },
    { 0 },
};

// Local time on the phone, and its time zone + DST, to Unix time (UTC).
static uint32_t to_unix(const uint8_t *c, int tz_15min, int dst_15min)
{
    struct tm t = {
        .tm_year = (c[0] | c[1] << 8) - 1900, .tm_mon = c[2] - 1, .tm_mday = c[3],
        .tm_hour = c[4], .tm_min = c[5], .tm_sec = c[6],
    };
    // timegm without the C library's TZ: days from the civil date.
    const int y = t.tm_year + 1900 - (t.tm_mon < 2), m = t.tm_mon + 1;
    const int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + t.tm_mday - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = (int64_t)era * 146097 + doe - 719468;
    const int64_t local = days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    return (uint32_t)(local - (int64_t)(tz_15min + dst_15min) * 900);
}

static void done(uint32_t unix_time)
{
    if (unix_time) {
        got_time = unix_time;
        read_ok = true;
    }
    finished_us = esp_timer_get_time();
    finished = true;
    if (conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
}

static int on_read_info(uint16_t c, const struct ble_gatt_error *err, struct ble_gatt_attr *attr, void *arg)
{
    int tz = 0, dst = 0;
    uint8_t v[2];
    if (err->status == 0 && attr && OS_MBUF_PKTLEN(attr->om) >= 2 && ble_hs_mbuf_to_flat(attr->om, v, 2, NULL) == 0) {
        tz = (int8_t)v[0];
        dst = v[1] == 255 ? 0 : v[1];
        if (tz == -128) tz = 0;   // unknown
    } else {
        ESP_LOGW(TAG, "no local time info (%d): taking the phone's time as UTC", err->status);
    }
    const uint32_t t = to_unix(cur, tz, dst);
    ESP_LOGI(TAG, "phone time %04d-%02d-%02d %02d:%02d:%02d, zone %+d min, DST %d min -> %lu",
             cur[0] | cur[1] << 8, cur[2], cur[3], cur[4], cur[5], cur[6], tz * 15, dst * 15, (unsigned long)t);
    done(t);
    return 0;
}

static int on_read_time(uint16_t c, const struct ble_gatt_error *err, struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0 || !attr || OS_MBUF_PKTLEN(attr->om) < 7 || ble_hs_mbuf_to_flat(attr->om, cur, sizeof(cur), NULL) != 0) {
        ESP_LOGW(TAG, "reading the time failed (%d)", err->status);
        done(0);
        return 0;
    }
    have_cur = true;
    if (info_handle) ble_gattc_read(conn, info_handle, on_read_info, NULL);
    else {
        static const struct ble_gatt_error none = { .status = BLE_HS_ENOENT };
        on_read_info(conn, &none, NULL, NULL);
    }
    return 0;
}

static int on_chr(uint16_t c, const struct ble_gatt_error *err, const struct ble_gatt_chr *chr, void *arg)
{
    if (err->status == 0 && chr) {
        const uint16_t u = ble_uuid_u16(&chr->uuid.u);
        if (u == CTS_CUR_TIME) cur_handle = chr->val_handle;
        if (u == CTS_LOCAL_INFO) info_handle = chr->val_handle;
        return 0;
    }
    if (err->status == BLE_HS_EDONE && cur_handle) {
        ble_gattc_read(conn, cur_handle, on_read_time, NULL);
    } else {
        ESP_LOGW(TAG, "the phone has no Current Time characteristic (%d)", err->status);
        done(0);
    }
    return 0;
}

static int on_svc(uint16_t c, const struct ble_gatt_error *err, const struct ble_gatt_svc *svc, void *arg)
{
    if (err->status == 0 && svc) {
        svc_start = svc->start_handle;
        svc_end = svc->end_handle;
        return 0;
    }
    if (err->status == BLE_HS_EDONE && svc_start) {
        ble_gattc_disc_all_chrs(conn, svc_start, svc_end, on_chr, NULL);
    } else {
        ESP_LOGW(TAG, "the phone offers no Current Time Service (%d)", err->status);
        done(0);
    }
    return 0;
}

static void read_phone_time(void)
{
    svc_start = svc_end = cur_handle = info_handle = 0;
    have_cur = false;
    ble_gattc_disc_svc_by_uuid(conn, BLE_UUID16_DECLARE(CTS_SVC), on_svc, NULL);
}

static int on_gap(struct ble_gap_event *ev, void *arg)
{
    struct ble_gap_conn_desc desc;
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status != 0) {
            advertise();
            return 0;
        }
        conn = ev->connect.conn_handle;
        state = BLE_TIME_CONNECTED;
        ESP_LOGI(TAG, "phone connected: pairing");
        // iOS only lets a paired accessory read its time.
        if (ble_gap_security_initiate(conn) != 0) read_phone_time();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "pairing %s (%d)", ev->enc_change.status == 0 ? "done" : "failed", ev->enc_change.status);
        read_phone_time();   // try anyway if it failed: some phones allow it
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        // Paired before (this board's bond store, or the phone forgot it): start again.
        if (ble_gap_conn_find(ev->repeat_pairing.conn_handle, &desc) == 0) ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "phone disconnected (%d)", ev->disconnect.reason);
        conn = BLE_HS_CONN_HANDLE_NONE;
        if (!finished) advertise();   // (sets WAITING; once finished it stays CONNECTED until switched off)
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (!finished && conn == BLE_HS_CONN_HANDLE_NONE) advertise();
        return 0;
    default:
        return 0;
    }
}

// Advertising as a remote: flags, appearance (generic HID) and the HID
// service, so iOS offers it in Settings > Bluetooth; the name in the scan
// response. (Soliciting Apple's notification service, tried first, didn't
// list it, 2026-11-14.)
static void advertise(void)
{
    struct ble_hs_adv_fields f = { 0 };
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.appearance = 0x0180;   // generic HID (a remote control is 0x0180 too)
    f.appearance_is_present = 1;
    f.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(0x1812) };
    f.num_uuids16 = 1;
    f.uuids16_is_complete = 1;
    ble_gap_adv_set_fields(&f);
    struct ble_hs_adv_fields rsp = { 0 };
    const char *name = ble_svc_gap_device_name();
    rsp.name = (uint8_t *)name;
    rsp.name_len = strlen(name);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);
    struct ble_gap_adv_params p = { .conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN };
    uint8_t own;
    ble_hs_id_infer_auto(0, &own);
    const int rc = ble_gap_adv_start(own, NULL, BLE_HS_FOREVER, &p, on_gap, NULL);
    if (rc && rc != BLE_HS_EALREADY) ESP_LOGW(TAG, "advertising failed (%d)", rc);
    state = BLE_TIME_WAITING;
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    advertise();
    ESP_LOGI(TAG, "advertising as \"%s\" until the time is set", ble_svc_gap_device_name());
}

static void on_reset(int reason) { ESP_LOGW(TAG, "Bluetooth host reset (%d)", reason); }

static void host_task(void *arg)
{
    nimble_port_run();   // returns after nimble_port_stop()
    nimble_port_freertos_deinit();
}

void ble_time_start(void)
{
    if (running) return;
    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth didn't start");
        return;
    }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;   // no keys or PIN on the base: Just Works
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_gatts_count_cfg(services) != 0 || ble_gatts_add_svcs(services) != 0) ESP_LOGW(TAG, "remote services not added");
    ble_svc_gap_device_name_set(BLE_TIME_NAME);
    ble_svc_gap_device_appearance_set(0x0180);
    ble_store_config_init();
    finished = read_ok = false;
    got_time = 0;
    conn = BLE_HS_CONN_HANDLE_NONE;
    started_us = esp_timer_get_time();
    running = true;
    state = BLE_TIME_WAITING;
    nimble_port_freertos_init(host_task);
}

static void stop(const char *why)
{
    if (!running) return;
    running = false;
    if (conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    ble_gap_adv_stop();
    if (nimble_port_stop() == 0) nimble_port_deinit();
    state = BLE_TIME_OFF;
    ESP_LOGI(TAG, "Bluetooth off: %s", why);
}

bool ble_time_poll(bool stop_now, uint32_t *unix_time)
{
    bool have = false;
    const uint32_t t = got_time;
    if (t) {
        got_time = 0;
        *unix_time = t;
        have = true;
    }
    if (!running) return have;
    // Done: once the phone has gone, or 3 s after asking it to go.
    if (finished && (conn == BLE_HS_CONN_HANDLE_NONE || esp_timer_get_time() - finished_us > 3000000LL))
        stop(read_ok ? "time read from the phone" : "the phone gave no time");
    if (running && stop_now && state != BLE_TIME_CONNECTED) stop("the time came another way");
    if (running && esp_timer_get_time() - started_us > BLE_TIME_WINDOW_MS * 1000LL && state != BLE_TIME_CONNECTED)
        stop("no phone in time");
    return have;
}

ble_time_state_t ble_time_state(void) { return state; }

#endif
