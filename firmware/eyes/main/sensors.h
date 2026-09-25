// Background tasks that sample the microphone and IMU and publish features.
#pragma once

#include "esp_err.h"
#include "audio_analysis.h"
#include "motion_analysis.h"

esp_err_t audio_start(void);
// Latest audio features. On the leader eye these come from the port eye while
// the link is up (returns true); otherwise from this eye's own mic.
bool audio_get(audio_features_t *out);

esp_err_t motion_start(void);
void motion_get(motion_features_t *out);
