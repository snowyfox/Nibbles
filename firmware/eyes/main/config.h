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

// ---------------------------------------------------------------- presets
#define PRESET_CYCLE_S       10      // move to the next visual preset this often (see presets.c)
#define SPIN_EASE_S          1.0f    // how quickly a spiral's spin follows tempo changes

// ---------------------------------------------------------------- eye geometry (pixels)
#define EYE_OUTLINE_RADIUS   218.0f  // fixed glowing ring around the whole eye
#define EYE_IRIS_RADIUS      150.0f  // outermost iris ring
#define EYE_PUPIL_RADIUS     38.0f   // base pupil radius
#define EYE_PUPIL_LOUD_GROW  16.0f   // extra pupil radius at full loudness
#define EYE_PUPIL_THUMP      14.0f   // extra pupil radius on a beat
#define EYE_MAX_LOOK_PX      70.0f   // how far the pupil can move from centre
#define EYE_MAX_RIPPLES      4

// ---------------------------------------------------------------- colour
#define HUE_DRIFT_DEG_PER_S  6.0f    // slow colour wander over time
#define HUE_WARM_DEG         330.0f  // target hue for bass-heavy music (magenta/red)
#define HUE_COOL_DEG         190.0f  // target hue for treble-heavy music (cyan)
#define HUE_PULL             0.6f    // 0 = ignore the music's colour, 1 = follow it fully
#define IDLE_INTENSITY       0.35f   // ring brightness with no music
#define SLEEP_INTENSITY      0.12f

// ---------------------------------------------------------------- audio
#define AUDIO_SAMPLE_RATE    16000
#define AUDIO_FRAME          512     // samples per analysis frame (32 ms)
// Automatic mic gain: the ES7210 gain is lowered fast when the signal nears
// clipping and raised quickly while it stays quiet. Analysis runs on levels with
// the gain removed, so gain changes don't disturb loudness or beat detection.
#define MIC_GAIN_START_DB    12.0f
#define MIC_GAIN_MIN_DB      0.0f
#define MIC_GAIN_MAX_DB      36.0f
#define MIC_GAIN_STEP_DB     3.0f
#define AGC_CLIP_DB          (-3.0f) // raw peak above this -> drop gain by two steps
#define AGC_TARGET_PEAK_DB   (-12.0f)// aim for raw peaks around here
#define AGC_RAISE_HOLD_S     0.5f    // peaks must stay 6 dB under target this long before raising
#define AGC_PEAK_RELEASE_DB_S 8.0f
#define AGC_SETTLE_FRAMES    4       // frames ignored after a gain change (~130 ms)

// Sleep needs a genuinely quiet room: the ~1.5 s average level (mic gain
// removed) must stay below QUIET_DB, with no steady beat, for SILENCE_SLEEP_S.
// Measured on this board at 36 dB mic gain: silent room -89 dB (-88 to -90),
// faint background music -74 to -84 dB. Raise QUIET_DB to sleep more readily,
// lower it (toward the silent-room level) to sleep less.
#define QUIET_DB             (-86.5f)
#define QUIET_AVG_S          1.5f
#define SILENCE_SLEEP_S      20.0f   // quiet this long -> eye gets drowsy
#define WAKE_MARGIN_DB       3.0f    // a sound this far above QUIET_DB wakes the eye...
#define WAKE_SOUND_S         0.15f   // ...if it lasts this long

// Background noise floor, used to gate loudness and beats. It never rises
// above QUIET_DB, so audible music is never mistaken for background.
#define NOISE_FLOOR_RISE_DB_S 0.3f
#define NOISE_FLOOR_LEARN_S  30.0f   // after boot the floor rises faster to learn the room
#define NOISE_FLOOR_LEARN_RISE_DB_S 1.0f
// Onset band weights for 40-160, 160-400, 400-1k, 1k-3k and 3k-8k Hz.
// Weighted toward the highs, where this board's mic hears the rhythm best.
#define ONSET_WEIGHTS        { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f }
// Beats: onsets above mean + K * stddev of the last ~1.5 s, fired only while
// the last ~4 s of onsets are rhythmic (periodic at 60-240 bpm). Beats follow
// the music's strong onsets, which can include accented off-beats.
#define BEAT_THRESHOLD_K     1.0f    // lower = more sensitive
#define BEAT_MIN_CONFIDENCE  0.4f    // how rhythmic the sound must be (0..1); keeps noise and talk out
#define BEAT_MIN_INTERVAL_S  0.25f
#define BEAT_MIN_BPM         60.0f
#define BEAT_MAX_BPM         240.0f
// The reported tempo is folded into this range, which covers the target
// genres at their natural tempo: EDM ~128, trance ~138, dubstep 140 (half-time
// 70 folds to 140), hardstyle 150, drum and bass 170-190.
#define TEMPO_FOLD_MIN_BPM   100.0f
#define TEMPO_FOLD_MAX_BPM   200.0f
#define BEAT_EVAL_FRAMES     8       // re-measure rhythm every ~0.25 s
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
#define LOOK_GYRO_GAIN       0.004f  // how hard tilting (deg/s) pushes the pupil
#define LOOK_DOWN_BIAS       0.25f   // eye tends to look toward the ground (the crowd)

// Twist: rotation about the vertical (the pole spinning the shark). It pushes
// the pupil sideways like other motion, but when the movement is mostly
// twist the left/right response reverses, so the eye looks into the turn.
#define TWIST_GAIN           0.006f  // sideways look per deg/s of twist
#define TWIST_SCREEN_SIGN    1.0f    // +1 when the screen faces outward from the pole's axis; flip
                                     // to -1 if twisting makes the eye look out of the turn
// Where the eye sits relative to the pole's twist axis. The two eyes are
// mounted back to back, forward of the pole, screens facing out to each side.
// Spinning pulls the eye back toward the axis (centripetal), which lies along
// the screen's horizontal and is removed from the accelerometer so it isn't
// taken for sway. (A recording confirmed that twist acceleration shows up
// through the screen, i.e. the eye is offset forward, not outward.)
#define EYE_FORWARD_M        0.12f   // eye centre forward of the axis, toward the nose
#define EYE_OUTWARD_M        0.0f    // eye centre outward of the axis, the way the screen faces
// Which eye is which, by board MAC address. The starboard screen's +x points
// toward the nose and the port screen's toward the tail. Boards not listed
// are treated as starboard (the original board, 28:84:85:3A:DA:78, is starboard).
#define PORT_EYE_MACS        { { 0x28, 0x84, 0x85, 0x3B, 0x6F, 0xF4 } }
#define TWIST_ATTACK_S       0.05f   // how quickly a new twist is recognised...
#define TWIST_SMOOTH_S       0.3f    // ...and how slowly it is forgotten
#define TWIST_SNAP_S         0.05f   // looking into a turn skips the pupil spring; this is its smoothing
#define TWIST_DOMINANT_LO    0.45f   // share of motion from twist where reversal starts...
#define TWIST_DOMINANT_HI    0.60f   // ...and where it is complete (hand twisting scores ~0.6-0.8)
#define DANCE_WINDOW_S       4       // seconds (integer: sizes a buffer)
#define DANCE_MIN_PERIOD_S   0.3f    // up to 200 bpm
#define DANCE_MAX_PERIOD_S   1.2f
#define DANCE_MIN_ENERGY_G   0.06f   // rms linear accel needed to count as dancing
#define DANCE_HYPE_SCORE     0.6f    // dance score where the eye goes into hype mode

// Vigorous motion of any kind (twisting, swinging, shaking) also drives hype.
// Activity is the stronger of rotation and linear acceleration over ~1 s,
// mapped 0..1 between these levels. Measured on the mount: slow twisting
// 70-120 deg/s and 0.05-0.12 g, fast twisting 270-450 deg/s and 0.23-0.40 g.
#define ACTIVITY_AVG_S       1.0f
#define ACTIVITY_GYRO_LO     150.0f  // deg/s rms
#define ACTIVITY_GYRO_HI     300.0f
#define ACTIVITY_ACCEL_LO    0.15f   // g rms
#define ACTIVITY_ACCEL_HI    0.30f
#define HYPE_ACTIVITY_HOLD_S 2.0f    // hype from motion lasts this long after the motion stops...
#define HYPE_ACTIVITY_FADE_S 1.5f    // ...then fades over this long

// ---------------------------------------------------------------- behaviour
#define BLINK_MIN_S          3.0f
#define BLINK_MAX_S          8.0f
#define BLINK_DURATION_S     0.18f
// Preset changes happen behind a slower, deliberate blink: close, hold shut
// (the preset swaps here), open.
#define SWAP_CLOSE_S         0.14f
#define SWAP_HOLD_S          0.08f
#define SWAP_OPEN_S          0.20f
#define DROWSY_CLOSE_S       5.0f    // time for lids to close once drowsy
#define WAKE_MOTION_G        0.35f   // a jolt this big wakes the eye too

// ---------------------------------------------------------------- hype (dancing)
#define HYPE_RING_SPEED      2.0f    // rings per second flowing outward when the tempo is unknown
#define HYPE_HUE_BOOST       6.0f    // colour cycles this many times faster at full hype
#define HYPE_WOBBLE_PX       6.0f    // extra ring wobble at full hype
#define HYPE_GLOW_BOOST      0.25f   // extra brightness at full hype
