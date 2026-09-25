// Background tasks that sample the microphone and IMU and publish features.
#pragma once

#include "esp_err.h"
#include "audio_analysis.h"
#include "motion_analysis.h"

#include "nibbles_link.h"

typedef enum { AUDIO_OWN_MIC, AUDIO_PORT_EYE, AUDIO_WLED } audio_source_t;

esp_err_t audio_start(void);
// Latest audio features. On the leader eye these come from WLED over the radio
// (with EYES_AUDIO_FROM_WLED), else from the port eye while the link is up,
// else from this eye's own mic. Returns where they came from.
audio_source_t audio_get(audio_features_t *out);
const char *audio_source_name(audio_source_t s);
// Audio features received over the radio from the WLED usermod.
void audio_set_radio(const nl_audio_t *a);

esp_err_t motion_start(void);
void motion_get(motion_features_t *out);
