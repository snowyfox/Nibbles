// Prints the eyes' built-in presets as eye looks in WLED preset JSON, ready to
// paste into a WLED preset ("nibbles":{"eyes":{"look":{...}}}), so WLED
// presets can carry the eyes' looks (see nl_eye_look_t in nibbles_link.h and
// the usermod's "eyes" settings).
//   make -C firmware/eyes/test/host looks
#include <stdio.h>
#include "presets.h"

static void num(const char *key, float v, int last) { printf("\"%s\":%g%s", key, v, last ? "" : ","); }

int main(void)
{
    printf("[\n");
    for (int i = 0; i < preset_count; i++) {
        nl_eye_look_t l;
        preset_to_look(&presets[i], 0, &l);
        printf("  {\"preset\":%d,\"look\":{\"name\":\"%s\",\"peak_beats\":%s,\"rainbow\":%s,\"bands\":%s,\"palette\":[",
               i + 1, l.name, l.flags & NL_EYE_LOOK_PEAK_BEATS ? "true" : "false",
               l.flags & NL_EYE_LOOK_RAINBOW ? "true" : "false", l.flags & NL_EYE_LOOK_BANDS ? "true" : "false");
        for (int c = 0; c < 4; c++) printf("[%d,%d,%d]%s", l.palette[c][0], l.palette[c][1], l.palette[c][2], c < 3 ? "," : "],");
        printf("\"rings\":%d,\"spiral_arms\":%d,", l.rings, l.spiral_arms);
        num("hue_spread", l.hue_spread, 0); num("sat", l.sat, 0);
        num("palette_step", l.palette_step, 0); num("palette_music", l.palette_music, 0);
        num("ring_width", l.ring_width, 0); num("halo_sigma", l.halo_sigma, 0); num("halo_amp", l.halo_amp, 0);
        num("fill", l.fill, 0); num("flow", l.flow, 0); num("wobble_px", l.wobble_px, 0);
        num("pupil_scale", l.pupil_scale, 0); num("rim_amp", l.rim_amp, 0); num("rim_white", l.rim_white, 0);
        num("ripple_pos", l.ripple_pos, 0); num("ripple_white", l.ripple_white, 0);
        num("spiral_twist", l.spiral_twist, 0); num("spin", l.spin, 0); num("spin_per_beat", l.spin_per_beat, 1);
        printf("}}%s\n", i < preset_count - 1 ? "," : "");
    }
    printf("]\n");
    return 0;
}
