// Tuning constants for the Nibbles eye. Everything you might want to adjust
// on site lives here.
#pragma once

// ---------------------------------------------------------------- display
#define DISP_W               466
#define DISP_H               466
#define DISP_CX              233.0f
#define DISP_CY              233.0f
#define DISP_RADIUS          233.0f
#define STRIP_ROWS           16      // must be even (CO5300 needs even y ranges)
#define STRIP_BUFFERS        3

// Brightness presets (percent) cycled by the BOOT button. Index 2 (100%, the
// panel's maximum) is the default.
#define BRIGHTNESS_PRESETS   { 30, 60, 100 }
#define BRIGHTNESS_DEFAULT_INDEX 2

// ---------------------------------------------------------------- eye geometry (pixels)
#define EYE_OUTLINE_RADIUS   218.0f  // fixed glowing ring around the whole eye
#define EYE_IRIS_RADIUS      150.0f  // outermost iris ring
#define EYE_PUPIL_RADIUS     38.0f   // base pupil radius
#define EYE_PUPIL_LOUD_GROW  16.0f   // extra pupil radius at full loudness
#define EYE_PUPIL_THUMP      14.0f   // extra pupil radius on a beat
#define EYE_RING_COUNT       5       // neon rings between pupil and iris edge
#define EYE_MAX_LOOK_PX      70.0f   // how far the pupil can move from centre
#define EYE_MAX_RIPPLES      4

// ---------------------------------------------------------------- colour
#define HUE_DRIFT_DEG_PER_S  6.0f    // slow colour wander over time
#define HUE_WARM_DEG         330.0f  // target hue for bass-heavy music (magenta/red)
#define HUE_COOL_DEG         190.0f  // target hue for treble-heavy music (cyan)
#define HUE_PULL             0.6f    // 0 = ignore the music's colour, 1 = follow it fully
#define HUE_RING_SPREAD_DEG  28.0f   // hue step between neighbouring rings
#define IDLE_INTENSITY       0.35f   // ring brightness with no music
#define SLEEP_INTENSITY      0.12f

// ---------------------------------------------------------------- audio
#define AUDIO_SAMPLE_RATE    16000
#define AUDIO_FRAME          512     // samples per analysis frame (32 ms)
#define MIC_GAIN_DB          6.0f    // ES7210 input gain. Keep low for festival volume.
#define SILENCE_DB           (-62.0f)// dBFS below which the eye counts it as quiet
#define SILENCE_SLEEP_S      20.0f   // quiet this long -> eye gets drowsy
#define BEAT_THRESHOLD_K     1.5f    // beat when bass flux > mean + K * stddev
#define BEAT_MIN_INTERVAL_S  0.25f
#define AGC_FLOOR_RISE_DB_S  1.5f    // how fast the noise floor creeps up
#define AGC_PEAK_FALL_DB_S   3.0f    // how fast the loudness peak decays
#define AGC_MIN_RANGE_DB     12.0f

// ---------------------------------------------------------------- motion
#define IMU_RATE_HZ          200
// Board-to-screen mapping. Rotate the IMU's x/y by this many degrees (0, 90,
// 180, 270) and flip signs until swinging the pole makes the pupil lag the
// right way. Set once when the board is mounted in the shark.
// Measured on the bench: the QMI8658 +X axis points toward the bottom of the screen.
#define EYE_MOUNT_ROTATION   90
#define IMU_X_SIGN           1.0f
#define IMU_Y_SIGN           1.0f
#define GRAVITY_TAU_S        0.6f    // low-pass time constant for the gravity estimate
#define LOOK_SPRING_HZ       1.4f    // natural frequency of the pupil spring
#define LOOK_DAMPING         0.35f   // damping ratio (lower = more wobble)
#define LOOK_ACCEL_GAIN      3.0f    // how hard linear acceleration pushes the pupil
#define LOOK_GYRO_GAIN       0.004f  // how hard rotation (deg/s) pushes the pupil
#define LOOK_DOWN_BIAS       0.25f   // eye tends to look toward the ground (the crowd)
#define DANCE_WINDOW_S       4       // seconds (integer: sizes a buffer)
#define DANCE_MIN_PERIOD_S   0.35f
#define DANCE_MAX_PERIOD_S   1.2f
#define DANCE_MIN_ENERGY_G   0.06f   // rms linear accel needed to count as dancing
#define DANCE_HYPE_SCORE     0.6f    // dance score where the eye goes into hype mode

// ---------------------------------------------------------------- behaviour
#define BLINK_MIN_S          3.0f
#define BLINK_MAX_S          8.0f
#define BLINK_DURATION_S     0.18f
#define DROWSY_CLOSE_S       5.0f    // time for lids to close once drowsy
#define WAKE_SOUND_S         0.3f    // sustained sound needed to wake up
#define WAKE_MOTION_G        0.35f   // a jolt this big wakes the eye too

// ---------------------------------------------------------------- hype (dancing)
#define HYPE_RING_SPEED      2.0f    // rings per second flowing outward when the tempo is unknown
#define HYPE_HUE_BOOST       6.0f    // colour cycles this many times faster at full hype
#define HYPE_WOBBLE_PX       6.0f    // extra ring wobble at full hype
#define HYPE_GLOW_BOOST      0.25f   // extra brightness at full hype
