// Updates asked for over the radio (2026-11-17), for boards with no keys or
// console in reach (the monitors, the leader eye). The base broadcasts
// NL_MSG_OTA_WIFI with a network's name and password; a board it names joins
// that network (ota_wifi_join) and, while it is on, broadcasts NL_MSG_OTA_STATE
// once a second: its IP, build and progress, for the base to list.
//
// Needs ota_wifi_init() before the radio starts, and the radio running.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "nibbles_link.h"

#define OTA_REMOTE_MINUTES 15   // on this long without an update, unless the base says otherwise

// my_target: NL_OTA_TO_EYES or NL_OTA_TO_MONITORS; name: how the base lists it.
void ota_remote_start(uint8_t my_target, const char *name);
// Call from the radio handler; true if the packet was one of these.
bool ota_remote_radio(const nl_radio_hdr_t *hdr, const uint8_t *payload, size_t len);
// Extra words in the state the base lists (the leader eye: the port eye's update).
void ota_remote_set_note(const char *note);
// For a screen: "" while Wi-Fi is off and nothing is being written.
void ota_remote_text(char *out, size_t len);
