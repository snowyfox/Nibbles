// Updates for the boards out of reach (2026-11-17): the base asks the monitors
// and the eyes over the radio to join its saved network (NL_MSG_OTA_WIFI) and
// lists what they report back (NL_MSG_OTA_STATE). Console:
//   ota remote                              who is on Wi-Fi, their IP and build
//   ota remote on [eyes|monitors|all] [MIN] join the network saved with "wifi set"
//   ota remote off [eyes|monitors|all]
//   ota wifi on|off, ota wifi                the base's own update (its address)
// All of these also work over the radio from a brain (its /keys/ask?c=...), so
// nothing needs USB: a computer on the Nibbles network can drive them.
// Then: python3 tools/base_ota.py wifi IP BIN (see shared/nibbles_ota).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "nibbles_link.h"

// From the radio handler.
void ota_peers_heard(const uint8_t mac[6], const nl_ota_state_t *s);
// A console line for these ("ota remote ...", "ota wifi ..."); false if it isn't one.
bool ota_peers_console(const char *line, void (*reply)(void *ctx, const char *line), void *ctx);
// Something new for the event line (a board got an address, an update ended).
bool ota_peers_event(char *out, size_t len);
