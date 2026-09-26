// Renders the cartoon animations on the host into a contact sheet (PPM):
// one row per animation, several moments each, at half size.
//   make -C test/host cartoon && ./test/host/build/cartoon_preview sheet.ppm
#include <stdio.h>
#include <stdlib.h>
#include "cartoon.h"
#include "config.h"

static void *alloc(size_t n) { return malloc(n); }

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "cartoon_sheet.ppm";
    const float times[] = { 0.12f, 1.3f, 2.7f, 4.15f };  // 0.12 s catches the Sparkle blink
    const int cols = sizeof(times) / sizeof(times[0]), rows = CARTOON_COUNT, s = 2;
    const int cw = DISP_W / s, ch = DISP_H / s, sw = cw * cols, sh = ch * rows;
    if (!cartoon_init(alloc)) return 1;
    static uint16_t frame[DISP_W * DISP_H];
    unsigned char *img = calloc((size_t)sw * sh * 3, 1);
    for (int a = 0; a < rows; a++) {
        for (int c = 0; c < cols; c++) {
            cartoon_prepare(a, times[c], false);
            cartoon_rows(frame, 0, DISP_H);
            for (int y = 0; y < ch; y++) {
                for (int x = 0; x < cw; x++) {
                    const uint16_t be = frame[(y * s) * DISP_W + x * s];
                    const uint16_t v = (uint16_t)((be >> 8) | (be << 8));
                    unsigned char *p = img + (((size_t)(a * ch + y) * sw) + c * cw + x) * 3;
                    p[0] = (unsigned char)(((v >> 11) & 31) * 255 / 31);
                    p[1] = (unsigned char)(((v >> 5) & 63) * 255 / 63);
                    p[2] = (unsigned char)((v & 31) * 255 / 31);
                }
            }
        }
        fprintf(stderr, "%d %s\n", a + 1, cartoon_name(a));
    }
    FILE *f = fopen(out, "wb");
    fprintf(f, "P6 %d %d 255\n", sw, sh);
    fwrite(img, 1, (size_t)sw * sh * 3, f);
    fclose(f);
    return 0;
}
