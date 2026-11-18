// Updates for the boards out of reach. See ota_peers.h.
#include "ota_peers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nibbles_espnow.h"
#include "ota.h"

#define PEERS 6
#define PEER_GONE_MS 5000

typedef struct {
    uint8_t mac[6];
    nl_ota_state_t s;
    int64_t heard_us;
} peer_t;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static peer_t peers[PEERS];
static char event[96];
static bool event_new;

static void set_event(const char *text)
{
    strlcpy(event, text, sizeof(event));
    event_new = true;
}

void ota_peers_heard(const uint8_t mac[6], const nl_ota_state_t *in)
{
    nl_ota_state_t s = *in;
    s.name[sizeof(s.name) - 1] = s.build[sizeof(s.build) - 1] = s.note[sizeof(s.note) - 1] = 0;
    const int64_t now = esp_timer_get_time();
    char text[96] = "";
    taskENTER_CRITICAL(&lock);
    peer_t *p = NULL, *oldest = &peers[0];
    for (int i = 0; i < PEERS; i++) {
        if (!memcmp(peers[i].mac, mac, 6)) { p = &peers[i]; break; }
        if (peers[i].heard_us < oldest->heard_us) oldest = &peers[i];
    }
    const bool fresh = !p || now - p->heard_us > PEER_GONE_MS * 1000LL;
    if (!p) { p = oldest; memset(p, 0, sizeof(*p)); memcpy(p->mac, mac, 6); }
    const nl_ota_state_t was = p->s;
    if (s.wifi == NL_OTA_WIFI_UP && (fresh || was.wifi != NL_OTA_WIFI_UP || memcmp(was.ip, s.ip, 4)))
        snprintf(text, sizeof(text), "%s: http://%d.%d.%d.%d/", s.name, s.ip[0], s.ip[1], s.ip[2], s.ip[3]);
    else if (!fresh && was.pct != NL_OTA_IDLE && s.pct == NL_OTA_IDLE)
        snprintf(text, sizeof(text), "%s: update sent, restarting", s.name);
    else if (s.note[0] && strcmp(was.note, s.note) && (fresh || !strstr(s.note, "%")))
        snprintf(text, sizeof(text), "%s: %s", s.name, s.note);
    p->s = s;
    p->heard_us = now;
    if (text[0]) set_event(text);
    taskEXIT_CRITICAL(&lock);
    if (text[0]) printf("OTA %s\n", text);
}

bool ota_peers_event(char *out, size_t len)
{
    taskENTER_CRITICAL(&lock);
    const bool got = event_new;
    if (got) strlcpy(out, event, len);
    event_new = false;
    taskEXIT_CRITICAL(&lock);
    return got;
}

// Answers go to the USB console or back over the radio (a brain's /keys/ask).
typedef void (*reply_fn)(void *ctx, const char *line);
static void say(reply_fn reply, void *ctx, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void say(reply_fn reply, void *ctx, const char *fmt, ...)
{
    char text[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    reply(ctx, text);
}

static uint8_t which(const char *word)
{
    if (!word || !*word || !strncmp(word, "all", 3)) return NL_OTA_TO_ALL;
    if (!strncmp(word, "eye", 3)) return NL_OTA_TO_EYES;
    if (!strncmp(word, "mon", 3)) return NL_OTA_TO_MONITORS;
    return 0;
}

static void list(reply_fn reply, void *ctx)
{
    const int64_t now = esp_timer_get_time();
    int shown = 0;
    peer_t copy[PEERS];
    taskENTER_CRITICAL(&lock);
    memcpy(copy, peers, sizeof(copy));
    taskEXIT_CRITICAL(&lock);
    for (int i = 0; i < PEERS; i++) {
        const peer_t *p = &copy[i];
        if (!p->heard_us || now - p->heard_us > PEER_GONE_MS * 1000LL) continue;
        char where[40];
        if (p->s.wifi == NL_OTA_WIFI_UP) snprintf(where, sizeof(where), "http://%d.%d.%d.%d/", p->s.ip[0], p->s.ip[1], p->s.ip[2], p->s.ip[3]);
        else snprintf(where, sizeof(where), "%s", p->s.wifi == NL_OTA_WIFI_JOINING ? "joining Wi-Fi" : "Wi-Fi off");
        char pct[16] = "";
        if (p->s.pct != NL_OTA_IDLE) snprintf(pct, sizeof(pct), ", updating %d%%", p->s.pct);
        say(reply, ctx, "OTA %-14s %s build %s%s%s%s  (%02X:%02X:%02X:%02X:%02X:%02X)", p->s.name, where, p->s.build, pct,
               p->s.note[0] ? ", " : "", p->s.note, p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
        shown++;
    }
    if (!shown) say(reply, ctx, "OTA no board on Wi-Fi (\"ota remote on [eyes|monitors|all]\")");
}

bool ota_peers_console(const char *line, void (*reply)(void *ctx, const char *line), void *ctx)
{
    // The base's own Wi-Fi update, also from the radio (no USB needed).
    if (!strcmp(line, "ota wifi on") || !strcmp(line, "ota wifi off")) {
        ota_wifi_set(line[10] == 'n');
        say(reply, ctx, "OTA base Wi-Fi %s%s", line[10] == 'n' ? "joining" : "off", line[10] == 'n' ? ": \"ota wifi\" for its address" : "");
        return true;
    }
    if (!strcmp(line, "ota wifi")) {
        char t[64];
        ota_wifi_text(t, sizeof(t));
        say(reply, ctx, "OTA base %s, build %s", t[0] ? t : "Wi-Fi off", ota_build_id());
        return true;
    }
    if (strncmp(line, "ota remote", 10) || (line[10] && line[10] != ' ')) return false;
    const char *rest = line + 10;
    while (*rest == ' ') rest++;
    if (!*rest) { list(reply, ctx); return true; }
    const bool on = !strncmp(rest, "on", 2) && (rest[2] == 0 || rest[2] == ' ');
    const bool off = !strncmp(rest, "off", 3) && (rest[3] == 0 || rest[3] == ' ');
    if (!on && !off) { say(reply, ctx, "OTA usage: ota remote [on|off] [eyes|monitors|all] [MINUTES]"); return true; }
    const char *arg = rest + (on ? 2 : 3);
    while (*arg == ' ') arg++;
    const uint8_t to = which(arg);
    if (!to) { say(reply, ctx, "OTA which boards? eyes, monitors or all"); return true; }
    const char *num = strchr(arg, ' ');
    if (!num && *arg >= '0' && *arg <= '9') num = arg;
    const unsigned minutes = num ? (unsigned)atoi(num) : 0;
    char ssid[33] = "", pass[65] = "";
    if (on && !ota_wifi_saved(ssid, pass)) { say(reply, ctx, "OTA no network saved: \"wifi set SSID PASSWORD\" first"); return true; }
    nl_ota_wifi_t m;
    if (!nl_ota_wifi_make(&m, to, on, (uint16_t)minutes, ssid, pass)) { say(reply, ctx, "OTA the saved network's name or password is too long"); return true; }
    for (int i = 0; i < 3; i++) {   // broadcasts can be missed: the boards take repeats
        nl_espnow_broadcast(NL_MSG_OTA_WIFI, &m, sizeof(m));
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    memset(&m, 0, sizeof(m));
    memset(pass, 0, sizeof(pass));
    say(reply, ctx, "OTA asked %s to %s%s; \"ota remote\" lists who answers", to == NL_OTA_TO_ALL ? "all" : to == NL_OTA_TO_EYES ? "the eyes" : "the monitors",
           on ? "join " : "leave Wi-Fi", on ? ssid : "");
    return true;
}
