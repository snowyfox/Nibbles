# Live / Standby monitors (2026-11-14)

Two Waveshare **ESP32-S3-Touch-AMOLED-1.64** boards in the base box, part of
replacing the 3.49 base board (too big) with smaller ones: the base's main
board becomes a 1.91 (`firmware/base`, `CONFIG_BASE_BOARD_AMOLED_191`) and
these two show Live and Standby. Radio only: no cable to the base.

| Board | MAC | Side |
|---|---|---|
| 1.64 #1 | 28:84:85:8D:40:4C | Live (default) |
| 1.64 #2 | 28:84:85:8D:4C:4C | Standby (`MONITOR_STANDBY_MACS`) |

`side live|standby` over USB overrides it (saved in NVS, restarts).

## Two modes: by side or by brain (2026-11-14)

The base chooses, for both monitors at once: its **MONITORS** key
(`KEY_MONITORS`, a key of its own to put in `KEYPAD_BOARDS`; its LED is grey
by side, blue by brain) or `monitors [sides|brains|toggle]` over its USB. The
base keeps the choice in NVS and sends it in every `NL_MSG_SHOW`
(`NL_SHOW_BY_BRAIN`).

- **By side** (the first design): each monitor shows a side, Live or Standby,
  whichever brain is on it; a TAKE + LATCH moves the pictures between screens.
- **By brain**: each monitor follows one brain. The Live-by-MAC board follows the
  **Right Brain** and the Standby one the **Left**, unless `brain right|left` over
  USB (saved, no restart) says otherwise. The banner says `RIGHT  LIVE` or
  `LEFT  STANDBY` and takes the colour of the brain's role (red, green), so a take
  swaps the banners, not the pictures. The brain is told by the preview's flags
  (`NL_PREVIEW_RIGHT` / `_LEFT`, set by brains with an `ab_side`, WLED build of
  2026-11-14), the role by the sender's radio role as before; the rows under it are
  that side's `NL_MSG_SHOW`. With the brain not heard: a grey banner, `RIGHT BRAIN`,
  "not heard".
- Tested on the bench 2026-11-14 (both brains on USB, no controller): both modes,
  switched back and forth from the base's serial; screenshots right. **Not tested:
  a take in the brain mode** (needs the controller and the brains' wires).

## Two pages: simple, and detail while the base's DETAIL key says so (2026-11-14)

Read from half a metre or more, so normally a **simple page**: a 56 px banner
(40 px title: LIVE / STANDBY with the brain's letter, or RIGHT LIVE / LEFT STANDBY in
the brain mode; LOCKED, LATCH or ON SHARK at 28 px on the right), the shark (a bit
lower), one big value on the right (Live: BRIGHT and the %; Standby: vs LIVE, NEW in
green or SAME dim) and the body preset at 40 px along the bottom. The detail page
below is shown while the base's DETAIL key is held, or after a tap until the next
(`NL_SHOW_DETAIL`); without the base, the simple page. 40 and 48 px fonts are in
`sdkconfig.defaults` (delete `sdkconfig` after changing them).

## The detail page (456 x 280, landscape)

Mounted in landscape (the user, 2026-11-14). The CO5300 can't turn the
picture, so LVGL draws 456 x 280 with `lv_display_set_rotation` (90 degrees,
or 270 after `flip` over USB, saved in NVS) and `flush_cb` turns each band
with `lv_draw_sw_rotate` into a DMA buffer before sending it. `shot` gives the
landscape picture. Layout: the banner across the top; the shark (296 x 150)
on the left; the target box, LOOK and brightness / "hold TAKE" on the right;
the body preset big along the bottom, eyes and fins under it, the radio line
last.


- (By side; see above for by brain.) A banner in the side's colour: LIVE (red) / STANDBY (green), the brain's
  letter with two brains (R / L), and LOCKED, LATCH, or ON SHARK (Standby
  while TAKE is held).
- The shark's side view from that side's brain (`NL_MSG_PREVIEW`: the Live
  brain sends as `NL_ROLE_WLED`, Standby as `NL_ROLE_WLED_STANDBY`), a 3 px
  dot per LED, `main/led_pos.h` (296 x 150, `tools/sim/make_preview_map.py`).
  Dim grey with "no ... brain heard" when there's no picture. With one brain
  the Standby monitor never has a picture.
- Under it, from the base (`NL_MSG_SHOW`, one packet per side, every 500 ms
  and on a change): body preset (big, two lines), eyes, fins, look (palette +
  layer), Live's brightness or Standby's "hold TAKE to show" / "same as Live".
  Standby that matches Live is dim. The keypad's target as a coloured box, and
  the radio channel and whether the picture arrives.

## Panel notes

**These are board v1.1** (the version silkscreen sits by the pin headers;
v1 has it at the top). Waveshare: v1 and v1.1 programs are not
interchangeable. v1.1 moved **LCD_CS to GPIO 46** (v1: 9) and IMU_INT1 to 9,
and added IMU_INT2 17, TP_INT 18, LCD_TE 45. With CS on 9 (every v1 example,
and Waveshare's own) the screens stay black: that cost an afternoon on
2026-11-14.

QSPI CS 46, CLK 10, D0-D3 11-14, reset 21; touch (FT3168) and the QMI8658 on
I2C 47/48 (unused). The panel is set up as Waveshare's 1.64 demo does:
Espressif's `esp_lcd_sh8601` driver with their list (0x11, 0xC4 0x80, 0x35,
0x53 0x20, 0x63 0xFF, 0x51, 0x29); brightness 0 until the first frame is out.
The CO5300's columns start **20 in** (`LCD_X_OFFSET`), windows must start
even and end odd (`round_area`), no hardware rotation. LVGL 9.2 in partial
mode, two 38-row DMA buffers.

Both panels lit and showing the monitor (the user, 2026-11-14, after the CS
fix). The screenshots (`shot`) come from LVGL's buffers, not the panel.

Build and flash: `idf.py -C firmware/monitor build`, then
`idf.py -C firmware/monitor -p PORT flash` (check the MAC first).

A lone brain with the controller unplugged sends as Standby
(`NL_ROLE_WLED_STANDBY`), so its picture shows on the Standby monitor, not
Live (seen 2026-11-14 with the Left Brain alone on USB).

## Updates without USB (2026-11-17)

- `partitions.csv` (the base's): two 6 MB app slots and rollback; one USB
  flash per monitor to change to it. Don't use `app-flash` afterwards.
- The base's `ota remote on monitors` makes both join its saved network for
  15 min; the footer shows `Update: http://IP/` in amber. Upload with
  `tools/base_ota.py wifi IP firmware/monitor/build/nibbles_monitor.bin`.
- The USB console takes the text protocol too (`ota status`, and
  `tools/base_ota.py usb PORT BIN`).
