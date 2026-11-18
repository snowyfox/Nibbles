// Updates without USB for the eyes (2026-11-17; shared/nibbles_ota).
// The leader eye takes a new image over Wi-Fi when the base asks for it
// (ota remote on eyes), and once that image has been kept (it ran 30 s and
// drew frames) it sends it to the port eye over the cable, NL_MSG_OTA_PART,
// whenever the port eye reports another build. So one upload updates both.
// The port eye never pushes anything; it only takes parts and answers.
#pragma once

#include "nibbles_link.h"

// Both eyes, at start-up (after link_start).
void eye_ota_start(void);
// From the link task: a part from the other eye.
void eye_ota_part(const nl_ota_part_t *m);
// The eye task calls this every frame, for the "is the new image working" check.
void eye_ota_frame(void);
