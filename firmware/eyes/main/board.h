// Which eye this board is, from its MAC address (see PORT_EYE_MACS).
#pragma once

#include "nibbles_link.h"

nl_side_t board_side(void);

// The port eye is the "ears" (mic and audio analysis); any other board leads
// (the shared eye brain, and later the eyes' radio).
nl_role_t board_role(void);
