// Updates without USB for the eyes. See eye_ota.h.
#include "eye_ota.h"

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "link.h"
#include "ota.h"
#include "ota_remote.h"

static const char *TAG = "eye_ota";

#define ASK_EVERY_MS     10000   // the leader asks the port eye's build this often
#define FIRST_ASK_S      45      // not before this long after boot (and the image kept)
#define BEGIN_WAIT_MS    30000   // the port eye erases its slot first
#define PART_WAIT_MS     300
#define PART_TRIES       5
#define MAX_FAILS        3       // pushes that failed before the leader stops trying (until a restart)

static volatile int64_t last_frame_us;
static QueueHandle_t parts;      // leader: answers from the port eye; port eye: requests for its task

void eye_ota_frame(void) { last_frame_us = esp_timer_get_time(); }

// A new image is kept only while the eye is drawing.
static bool drawing(void) { return esp_timer_get_time() - last_frame_us < 1000000; }

void eye_ota_part(const nl_ota_part_t *m)
{
    const bool reply = m->op & NL_OTA_REPLY;
    if (parts && reply == (board_role() == NL_ROLE_EYE_LEADER)) xQueueSend(parts, m, 0);
}

// ------------------------------------------------------------------ port eye: takes parts
static void port_task(void *arg)
{
    static nl_ota_part_t m, r;
    for (;;) {
        xQueueReceive(parts, &m, portMAX_DELAY);
        memset(&r, 0, NL_OTA_PART_HEADER_LEN);
        r.op = m.op | NL_OTA_REPLY;
        r.status = NL_OTA_ST_OK;
        esp_err_t err = ESP_OK;
        uint32_t next = 0;
        switch (m.op) {
        case NL_OTA_ASK:
            r.status = ota_busy() ? NL_OTA_ST_BUSY : NL_OTA_ST_OK;
            r.len = 16;
            memcpy(r.data, ota_build_id(), 16);
            break;
        case NL_OTA_BEGIN:
            err = ota_part_begin(m.off, "the leader eye");
            break;
        case NL_OTA_DATA:
            err = ota_part_write(m.off, m.data, m.len, &next);
            r.off = next;
            break;
        case NL_OTA_END:
            err = ota_part_end();   // restarts into it half a second later
            break;
        case NL_OTA_ABORT:
            ota_part_abort();
            break;
        default:
            continue;
        }
        if (err != ESP_OK) {
            r.status = NL_OTA_ST_ERR;
            ESP_LOGW(TAG, "update part %d at %u: %s", m.op, (unsigned)m.off, esp_err_to_name(err));
        }
        link_send_ota(&r);
    }
}

// ------------------------------------------------------------------ leader: pushes its image
static bool ask(nl_ota_part_t *m, nl_ota_part_t *r, int wait_ms, int tries)
{
    for (int t = 0; t < tries; t++) {
        xQueueReset(parts);
        link_send_ota(m);
        const int64_t until = esp_timer_get_time() + wait_ms * 1000LL;
        while (esp_timer_get_time() < until) {
            if (xQueueReceive(parts, r, pdMS_TO_TICKS(wait_ms)) != pdTRUE) break;
            if (r->op == (m->op | NL_OTA_REPLY)) return true;
        }
    }
    return false;
}

static bool push(void)
{
    const void *p;
    size_t size;
    if (!ota_running_image(&p, &size)) { ota_remote_set_note("port eye: no image to send"); return false; }
    const esp_partition_t *part = p;
    static nl_ota_part_t m, r;
    ESP_LOGI(TAG, "sending this image (%u bytes, build %s) to the port eye", (unsigned)size, ota_build_id());
    ota_remote_set_note("port eye: erasing");
    memset(&m, 0, sizeof(m));
    m.op = NL_OTA_BEGIN;
    m.off = size;
    if (!ask(&m, &r, BEGIN_WAIT_MS, 1) || r.status != NL_OTA_ST_OK) {
        ota_remote_set_note("port eye: didn't start");
        return false;
    }
    const int64_t t0 = esp_timer_get_time();
    int shown = -1;
    for (uint32_t off = 0; off < size;) {
        const size_t n = size - off < NL_OTA_PART_MAX ? size - off : NL_OTA_PART_MAX;
        m.op = NL_OTA_DATA;
        m.off = off;
        m.len = (uint16_t)n;
        if (esp_partition_read(part, off, m.data, n) != ESP_OK) { ota_remote_set_note("port eye: read failed"); goto fail; }
        if (!ask(&m, &r, PART_WAIT_MS, PART_TRIES) || r.status != NL_OTA_ST_OK || r.off != off + n) {
            char note[32];
            snprintf(note, sizeof(note), "port eye: failed at %u%%", (unsigned)(off * 100 / size));
            ota_remote_set_note(note);
            goto fail;
        }
        off += n;
        const int pct = (int)(off * 100 / size);
        if (pct / 5 != shown) {
            shown = pct / 5;
            char note[32];
            snprintf(note, sizeof(note), "port eye %d%%", pct);
            ota_remote_set_note(note);
        }
    }
    m.op = NL_OTA_END;
    m.len = 0;
    m.off = 0;
    if (!ask(&m, &r, 5000, 1) || r.status != NL_OTA_ST_OK) { ota_remote_set_note("port eye: image refused"); return false; }
    ESP_LOGI(TAG, "port eye updated in %.1f s: it restarts into the new image",
             (esp_timer_get_time() - t0) / 1e6);
    ota_remote_set_note("port eye updated");
    return true;
fail:
    m.op = NL_OTA_ABORT;
    m.len = 0;
    ask(&m, &r, PART_WAIT_MS, 2);
    return false;
}

static void leader_task(void *arg)
{
    static nl_ota_part_t m, r;
    int fails = 0;
    bool said_same = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(ASK_EVERY_MS));
        // Only an image that is kept goes on to the port eye, and not while this one is being updated.
        if (esp_timer_get_time() < FIRST_ASK_S * 1000000LL || !ota_image_kept() || ota_busy() || !link_peer_up()) continue;
        memset(&m, 0, sizeof(m));
        m.op = NL_OTA_ASK;
        if (!ask(&m, &r, 1000, 2)) continue;   // an older port eye that doesn't know about updates: nothing to do
        if (r.status == NL_OTA_ST_BUSY) { ota_remote_set_note("port eye: busy"); continue; }
        if (r.len >= 16 && !memcmp(r.data, ota_build_id(), 16)) {
            if (!said_same) {
                ESP_LOGI(TAG, "port eye runs the same build (%s)", ota_build_id());
                ota_remote_set_note("port eye: same build");
                said_same = true;
            }
            continue;
        }
        said_same = false;
        if (fails >= MAX_FAILS) { ota_remote_set_note("port eye: gave up"); continue; }
        ESP_LOGI(TAG, "port eye runs build %.16s, this one %s", (const char *)r.data, ota_build_id());
        if (!push()) {
            fails++;
            ESP_LOGW(TAG, "sending to the port eye failed (%d of %d)", fails, MAX_FAILS);
        }
    }
}

void eye_ota_start(void)
{
    ota_set_good_check(drawing);
    ota_start();
    parts = xQueueCreate(board_role() == NL_ROLE_EYE_LEADER ? 2 : 4, sizeof(nl_ota_part_t));
    if (board_role() == NL_ROLE_EYE_LEADER) xTaskCreatePinnedToCore(leader_task, "eye_ota", 4096, NULL, 3, NULL, 0);
    else xTaskCreatePinnedToCore(port_task, "eye_ota", 4096, NULL, 3, NULL, 0);
}
