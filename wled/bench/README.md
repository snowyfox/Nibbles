# WLED effect and layer benchmarks (brain, 2026-10-29)

Measured on the bench 3.49 brain (ESP32-S3, WLED 16.0.1, 95x41 matrix, 1064
LEDs streamed over DDP to the controller). The frame cap was lifted for the
tests (target fps 250), so the frame rate shows the cost. Music was playing for
almost the whole run, so the sound-reactive effects were doing real work.

How to read the numbers:

- A frame with nothing to do costs **10.1 ms** (99 fps). That's blending
  segment 0, applying the ledmap, and sending the DDP stream (plus the
  simulator's mirror stream). A plain Solid colour, timed every 15 effects,
  stayed between 97.5 and 101 fps all through the run.
- `effects.csv` lists every effect running alone on the body segment (70x41):
  its fps, its extra cost per frame on top of the 10.1 ms, a cost tier, and how
  long it takes to spin up from black.
  - **Tiers:** cheap < 2 ms (104 effects), medium 2-6 ms (75), heavy 6-15 ms
    (30), very heavy > 15 ms (7: Pacifica 81 ms, PS Hourglass, Distortion Waves,
    Noise 1, Aurora, Tartan, Noisefire).
  - A full-width layer (95x41) costs about 1.35x the listed figure, since it
    has that many more cells.
- **Spin-up:** each effect starts from black, and its brightness is followed
  frame by frame for 9 s, twice. The figure is when it holds 80% of its
  later level.
  - "instant": full on from the first frames.
  - "sparse": mostly black by design (sparkles, fireworks, strobes), so it
    can't be judged this way.
  - "varies": the two runs disagree, because of a slow pattern or the music.

## Layer costs (`layers.jsonl`, cases in `cases.json`)

| Case | fps | ms/frame |
|---|---|---|
| Body only (Solid) | 99.5 | 10.1 |
| + 1 full-size Solid layer, on | 69.5 | 14.4 |
| + 4 Solid layers, on | 33 | 30.3 |
| + 8 Solid layers, on | 21 | 47.6 |
| + 4 Solid layers, **off** | 78 | 12.8 |
| + Soap layer, on (Top or Stencil) | 31 | 32.3 |
| + Soap layer, on (Screen) | 29 | 34.5 |
| + Soap layer, **off** | 35 | 28.6 |
| + Soap layer, **off and frozen** | 101 | 9.9 |
| + 4 Soap layers off / off and frozen | 13 / 100 | 76.9 / 10.0 |
| Colorloop + Twinkle | 71.5 | 14.0 |
| Octopus + Twinkle | 47 | 21.3 |
| Fire 2012 + Octopus | 31 | 32.3 |
| Soap + Twinkle + Juggle | 29 | 34.5 |
| Soap + Noise 2 + Plasmoid | 17 | 58.8 |
| 5 slots: Octopus, Twinkle, Fire 2012, Juggle, bump (off) | 23 | 43.5 |
| Distortion Waves alone | 29 | 34.5 |

- **Combining a full-size layer costs about 4.7 ms**, whatever its effect is
  (Top and Stencil cost the same; Screen about 2 ms more). That's about
  1.2 us per cell. WLED keeps the frame being combined in PSRAM, which
  probably explains much of it (not profiled).
- **A layer that's off still runs its effect:** 0.7 ms for Solid, the full
  ~18 ms for Soap. **Frozen layers cost nothing.**
- At the normal 42 fps (23.8 ms per frame), there's about 13.7 ms left after
  the 10.1 ms base. Each active full-size layer takes 4.7 ms plus its effect.

Scripts: `fxbench.py` (cost and spin-up for every effect; needs the simulator
server mirroring the brain), `curves.py` (9 s brightness curves, saved in
`curves.json`), `layerbench.py` (the layer cases). Back the brain up before
running them, and restore its fps cap (42), brightness and segments afterwards.

## Layer size and memory (`sizes.jsonl`, `dram.jsonl`)

Layer size (`layerbench2.py` with `cases2.json`; each layer is anchored at the
top left):

| Case | ms/frame | Extra |
|---|---|---|
| Body segment 10 / 35 / 70 / 95 wide, Solid | 6.5 / 7.6 / 10.1 / 11.1 | - |
| + Solid layer 12x10 / 24x21 / 48x21 / 48x41 / 70x41 / 95x41 | 10.1 / 10.6 / 11.1 / 12.2 / 13.2 / 14.3 | 0 / 0.5 / 1.0 / 2.2 / 3.1 / 4.2 ms |
| + Soap layer 48x21 / 95x41 | 15.2 / 32.3 | 5.1 / 22.2 ms |
| + Noise 2 layer 48x21 / 95x41 | 14.5 / 29.4 | 4.4 / 19.4 ms |

- **Fixed cost:** about 6 ms per frame, whatever the layers. That's the output
  pass over the whole 95x41 frame and the DDP send.
- **Everything else scales with the cells a layer covers:** about 1.1 us per
  cell to render a Solid layer and combine it. An effect's own cost scales
  the same way, or a bit better.
- The 95x41 grid has 3,895 cells for 1,064 LEDs. A grid about the size of the
  LED count would make every layer and effect about 3.5-4x cheaper.

**Internal RAM instead of PSRAM:** I flashed a test build that put the
combined frame (`_pixels`) in internal RAM (`BFRALLOC_ENFORCE_DRAM`) and
preferred internal RAM for the segment buffers. It made no difference (every
case within a frame or two, `dram.jsonl`). The S3's cache hides PSRAM for this
access pattern, so the cost is the per-cell work itself. The brain is back on
its normal build.

## A coarser grid (`grid60.jsonl`, `cases4.json`, `ledmap_60x28/`)

`make_ledmap.py --cells 1100 --fin-panel-mm 20` gives a 60x28 grid (1,680 cells,
13.2 mm each; body 47x28, fin panel 13x18), the smallest the generator can fit
every LED into. I loaded it on the brain for the test and then put the 95x41
map back. Same cases as above, with the layers covering the whole 60x28 grid:

| Case | 95x41 fps | 60x28 fps |
|---|---|---|
| Body only (Solid) | 99.5 | 146.5 |
| + 1 / 4 / 8 Solid layers | 69.5 / 33 / 21 | 116 / 69 / 42 |
| + Soap layer | 31 | 62.5 |
| Octopus + Twinkle | 47 | 87 |
| Fire 2012 + Octopus | 31 | 62 |
| Soap + Twinkle + Juggle | 29 | 58 |
| Soap + Noise 2 + Plasmoid | 17 | 34 |
| 5 slots: Octopus, Twinkle, Fire 2012, Juggle, bump (off) | 23 | 45 |
| 5 slots heavy: Soap, Twinkle, Noise 2, Juggle, bump (off) | 19 | 38 |
| Distortion Waves alone | 29 | 55 |

About twice as fast everywhere: the realistic 5-slot stack goes from 23 fps
to 45, enough for the 42 fps cap.

**The cost is placement.** With bigger cells, LEDs that sit close together
compete for the same cell, so some move several cells from where they really
are. The preview SVG shows how far each one moved, and 2D effects look
coarser. This needs checking by eye on the shark before choosing it.
