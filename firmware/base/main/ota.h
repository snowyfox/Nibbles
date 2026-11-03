// Firmware updates without USB (2026-11-02). Two app slots (partitions.csv);
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
// This image's identity: the first 16 hex digits of its ELF SHA-256.
const char *ota_build_id(void);
// The running image's size and slot, for passing it on to the mirror.
bool ota_running_image(const void **partition, size_t *size);
// Something is writing an update (the screen says so; the mirror link waits).
bool ota_busy(void);
// Progress for the screen: "" when idle.
void ota_progress_text(char *out, size_t len);

// Over Wi-Fi (main board only): join the network saved with "wifi set" and
// take the image as an HTTP POST to /ota. Off until asked for.
void ota_wifi_init(void);          // before the radio starts (creates the Wi-Fi netif)
void ota_wifi_set(bool on);
bool ota_wifi_on(void);
bool ota_wifi_save(const char *ssid, const char *pass);
void ota_wifi_text(char *out, size_t len);   // for the screen: "" when off
