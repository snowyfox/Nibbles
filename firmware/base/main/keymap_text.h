// A key board's switch-to-key list as text ("1,2,take,none,..."), as the base
// keeps it in NVS and the `keymap` serial command shows it. No ESP-IDF calls:
// host-tested in test/host.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "controls.h"

// Writes the n keys' names, comma-separated ("none" for KEY_NONE). Returns the
// length, or -1 if it doesn't fit.
int keymap_format(const ctl_key_t *keys, int n, char *out, size_t size);

// Reads exactly n comma-separated names (as `keys` lists them, or "none") into
// keys. Returns false, leaving keys as they were, if a name is unknown or the
// count is wrong.
bool keymap_parse(const char *text, ctl_key_t *keys, int n);

// One name, or "none": sets *key and returns true; false for an unknown name.
bool keymap_key(const char *name, ctl_key_t *key);
