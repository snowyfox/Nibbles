// The cable between the main 3.49 (the base) and a mirror 3.49 up the pole
// (2026-11-02). Both run the same firmware; the role is saved in NVS.
//
// The main board asks "hello" every MIRROR_HELLO_MS; the mirror answers
// "MIRROR <build> <ready|busy>" and never speaks unasked. When the mirror's
// build differs from the main's, the main sends it its own running image with
// the ota.h protocol, so one update to the main updates both.
#pragma once

#include <stdbool.h>
#include <stddef.h>

bool mirror_is_mirror(void);        // this board's saved role
bool mirror_set_role(bool mirror);  // saved; takes effect after a restart

// Main board.
bool mirror_link_enabled(void);     // saved: "mirror link on|off"
bool mirror_link_enable(bool on);   // saved; takes effect after a restart
void mirror_link_main_start(void);  // only if enabled
void mirror_link_push(void);        // send the image now, even if the builds match
void mirror_link_text(char *out, size_t len);   // for the console and the screen

// Mirror board.
void mirror_link_side_start(void);
bool mirror_main_heard(void);       // the main asked in the last few seconds
