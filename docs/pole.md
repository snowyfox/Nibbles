# The pole and its foot adapter

The shark rides on a **K-Tek K-123** (Klassic series, six-section graphite
boom pole, discontinued). The base enclosure attaches to the pole through a
**modified K-Tek KFT** flow-through foot. Both are modelled in the user's
Fusion 360 design. STEP exports are in `hardware/pole/`:

| File | What |
|---|---|
| `K-Tek K123 Boompole.step` | the pole, collapsed, foot down (z = 0 at the bottom) |
| `KFT.step` | the KFT as bought |
| `KTEKFT Lower.step` | the KFT's lower half after the cut |
| `KTEKFT Upper.step` | the KFT's upper half after the cut, drilled and tapped |

The three KFT files share one coordinate frame (z = 0 at the bottom of the
mushroom base), so the halves sit exactly where they were in the uncut part.
The measurements below come from these files (FreeCAD, 2026-10-29). They are
in mm and rounded to 0.1. Check against the real parts anything that has to fit
tightly.

## K-123

Retailer specs (sources disagree slightly): 76 cm (2'6") collapsed, 3.31 m
(10'3") extended, 580 g. Don't confuse it with the K-123CCR, the version with
a coiled cable inside (2'9"–10'5", 830 g).

From the model:

- **Collapsed length:** 739.2 mm, from the bottom of the foot to the tip of
  the thread.
- **Bottom section:** 551 mm from the floor to the top of its collar. That's
  a 31.8 mm foot, a 487 mm tube and a 31.8 mm collar. It's shorter than the
  60–80 cm guess. The upper keypad goes near the top of this section.
- **Tip:** a 9.4 mm × 11.5 mm stud, which fits the standard 3/8"-16 boom
  thread. Below it is a 24.9 mm cap, 47 mm long.

| Section (bottom → top) | Tube OD | Collar OD |
|---|---|---|
| 1 (bottom) | 39.7 | 43.3 |
| 2 | 33.3 | 39.7 |
| 3 | 30.2 | 36.6 |
| 4 | 27.0 | 33.0 |
| 5 | 23.8 | 30.0 |
| 6 (top) | 20.6 | 24.9 cap |

The model's inner bores are simplified. The inner tubes overlap the bores of
the tubes around them, so take only outside diameters from it. When the pole
is collapsed, the inner sections reach down to about 140–180 mm above the
floor. Anything that goes inside the bottom tube, such as cables, has to
stay below that.

## KFT, as bought

- **Overall:** 215.9 mm long (8.5"), with a 26.5 mm bore all the way through.
- **Bottom of the bore:** a 31.7 mm counterbore, 6.8 mm deep.
- **Outside diameter, from the bottom up:**

  | Height (mm) | OD (mm) |
  |---|---|
  | 0–12.2 | 36.9 (mushroom base) |
  | 12.2–24.2 | 35.2 |
  | 24.2–150.3 | 32.9 |
  | 150.3–153.4 | step to 31.5 |

- **Top:** a 29.9 mm socket from 153.4 mm to the top (62.5 mm deep).
- **Cable slot:** goes right through the part (open on both sides, ±Y). It
  runs from z ≈ 33 mm to ≈ 136 mm, about 103 mm long (the "4 inch slot").
  Its ends curve out as arcs of 30 mm diameter.

## The modification

The KFT was cut in two through the slot. The dimples on the sides of the
slot section were drilled out and tapped, so screws hold the base enclosure
to it.

- **Lower half:** z 0–61.7 mm. Its mushroom base is turned down slightly, to
  35.8 mm OD (36.9 originally). It keeps the bottom 29 mm of the slot
  (z ≈ 33–61.7) and has no tapped holes.
- **Removed:** z 61.7–92.8 mm, 31.1 mm in all, plus 2.6 mm off the top
  (the upper half ends at 213.3 mm).
- **Upper half:** z 92.8–213.3 mm (120.5 mm long). It keeps the top of the
  slot (z 92.8 to ≈ 136) and the pole socket.
- **Tapped holes:** six, all in the upper half. There are three on each side,
  on the faces between the slot openings (±X), so they sit 90° from the slot.
  - The holes are modelled at 6.0 mm diameter. Check the thread size on the
    part: it could be M6 or 1/4-20, and a tap-drill size would be smaller.
  - They go through the 1.7 mm wall at z = 99.8, 115.6 and 131.3 mm. That's
    7.0, 22.8 and 38.5 mm from the cut end, about 15.7 mm apart.
