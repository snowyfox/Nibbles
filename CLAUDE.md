# Nibbles

A festival totem: a shark on top of a 3 m pole that looks like it swims in the
sky and dances to the music (trance, EDM, dubstep, drum and bass; up to 180+
bpm). Several ESP32 boards work together as one system. This repo holds all
of Nibbles' own code; WLED itself stays upstream.

## The pieces

| Part | Hardware | Code | Status |
|---|---|---|---|
| **Eyes** (port + starboard) | 2× Waveshare ESP32-S3-Touch-AMOLED-1.75, back to back | `firmware/eyes/` (ESP-IDF 5.5.5) | Working; each eye standalone |
| **Lights** | Existing ESP32 running WLED (0.15, to be upgraded to 16.x), external digital mic | `wled/usermod_nibbles/` (planned) | Stock WLED today |
| **Base station** | New ESP32 in the pole base: batteries, status screen(s), buttons | `firmware/base/` (planned) | Not built; hardware not chosen |
| **Shared protocol** | used by all of the above | `shared/nibbles_link/` (planned) | Not written |

`docs/architecture.md` has the agreed design and roadmap: roles, the wired eye
link, ESP-NOW with WLED as the channel anchor, the message rules, and phases.
Each sub-project has its own `CLAUDE.md` with details, loaded automatically
when working in that directory. The eyes' one is the most complete record of
how the firmware works and why.

## Boards

| Board | MAC / USB serial | Role |
|---|---|---|
| Starboard eye | 28:84:85:3A:DA:78 | original board; planned link leader + radio gateway |
| Port eye | 28:84:85:3B:6F:F4 | recognised by MAC (`PORT_EYE_MACS`); planned audio source |

The USB port name varies (`/dev/cu.usbmodem201`, `…401`): `ls /dev/cu.usbmodem*`.
Check which board is connected by its serial number before flashing
(`system_profiler SPUSBDataType | grep -A6 JTAG`).

## Common commands

```sh
source ~/.espressif/tools/activate_idf_v5.5.5.sh     # ESP-IDF (EIM install; export.sh does not work here)
make -C firmware/eyes/test/host                       # eye host tests (no hardware)
idf.py -C firmware/eyes build
idf.py -C firmware/eyes -p /dev/cu.usbmodemNNN flash
```

## Conventions

- Only commit when asked, and commit **unsigned** (`git commit --no-gpg-sign`);
  the user's git config signs by default.
- Work in small verified steps: host tests, then build, then flash, then read
  the board's once-a-second status log. The user checks anything visual or
  physical; ask them to confirm.
- Prefer measuring on real hardware and real recordings over guessing (see the
  recording and replay tools in `firmware/eyes/CLAUDE.md`).
- Anything that more than one board must agree on goes in the shared protocol,
  not copied into each firmware.
