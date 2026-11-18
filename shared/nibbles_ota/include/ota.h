// Firmware updates without USB (2026-11-02; shared by the base, the monitors
// and the eyes since 2026-11-17). Two app slots (partitions.csv);
// a new image is written to the slot not running, then booted, and kept only
// if it runs for OTA_GOOD_AFTER_S (otherwise the bootloader goes back).
//
// The same text protocol works over the USB console (tools/base_ota.py) and
// over the cable from the main board to the mirror board (mirror_link.c):
//   ota begin SIZE             -> OTA READY <slot>        (after erasing)
//   ota data OFF CRC32 BASE64  -> OTA OK <next offset>    (CRC of the raw bytes, hex)
//   ota end                    -> OTA DONE                (then a restart)
//   ota abort                  -> OTA ABORTED
//   ota status                 -> OTA <state> ...
// Any failure answers "OTA ERR <why>".
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define OTA_GOOD_AFTER_S 30     // a new image that runs this long is kept
#define OTA_GOOD_RETRY_S 5      // ... or, if its good check says no, looked at again this often
#define OTA_GOOD_GIVE_UP_S 90   // ... and given up on (back to the old image) after this much longer
#define OTA_CHUNK 768           // raw bytes per "ota data" line (1024 base64 characters)
#define OTA_LINE_MAX 1200       // longest line the protocol sends

typedef void (*ota_reply_t)(void *ctx, const char *line);

// The standard CRC-32 (as zlib's crc32), for "ota data" lines.
#include <stdint.h>
uint32_t ota_crc32(const void *data, size_t len);

// Handles a line starting "ota "; false if it isn't one. Replies through reply.
bool ota_command(const char *line, ota_reply_t reply, void *ctx);
// Call once at start-up: marks this image good after OTA_GOOD_AFTER_S.
void ota_start(void);
// What this board is called on its web page, and its image's file name.
void ota_set_name(const char *name, const char *bin);
// Optional: a new image is kept only once this says it works (e.g. the eyes
// are drawing frames); asked again every OTA_GOOD_RETRY_S, and after
// OTA_GOOD_GIVE_UP_S of "no" the board goes back to the old image.
void ota_set_good_check(bool (*check)(void));
// This image is kept (not a new one still on trial).
bool ota_image_kept(void);
// This image's identity: the first 16 hex digits of its ELF SHA-256.
const char *ota_build_id(void);
// The running image's size and slot, for passing it on to the mirror.
bool ota_running_image(const void **partition, size_t *size);
// Something is writing an update (the screen says so; the mirror link waits).
bool ota_busy(void);
// Progress for the screen: "" when idle.
void ota_progress_text(char *out, size_t len);
int ota_progress_pct(void);     // -1 when idle

// The binary protocol (NL_MSG_OTA_PART on the eye cable): the same steps as
// the text one. ota_part_write takes the offset it expects (a repeat of the
// last part is answered again) and gives the next one; ota_part_end restarts
// the board into the new image on success.
#include "esp_err.h"
esp_err_t ota_part_begin(size_t size, const char *from);
esp_err_t ota_part_write(uint32_t off, const void *data, size_t n, uint32_t *next);
esp_err_t ota_part_end(void);
void ota_part_abort(void);

// Over Wi-Fi: join a network and take the image as an HTTP POST to /ota.
// Off until asked for: the network saved with "wifi set" (ota_wifi_set), or
// one named over the radio (ota_wifi_join, ota_remote.h), which goes off
// again after `minutes` (0: never) unless an update is being written.
void ota_wifi_init(void);          // before the radio starts (creates the Wi-Fi netif)
void ota_wifi_set(bool on);
void ota_wifi_join(const char *ssid, const char *pass, unsigned minutes);
bool ota_wifi_saved(char ssid[33], char pass[65]);
int ota_wifi_ip(uint8_t ip[4]);    // nl_ota_wifi_state_t: 0 off, 1 joining, 2 up (ip set)
bool ota_wifi_on(void);
bool ota_wifi_save(const char *ssid, const char *pass);
void ota_wifi_text(char *out, size_t len);   // for the screen: "" when off
