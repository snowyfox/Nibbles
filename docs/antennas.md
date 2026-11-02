# Antennas

Notes from 2026-11-02. Nothing is bought or modified yet. The plan is to try
one pair first in the mounting radio test (compare against the onboard
antennas) before buying the rest.

## Base: the 3.49's IPEX connector

The Waveshare ESP32-S3-Touch-LCD-3.49 has the ESP32-S3 chip on the board (no
module), with its own antenna circuit. From the [schematic](https://www.waveshare.net/w/upload/2/20/ESP32-S3-Touch-LCD-3.49-Schematic.pdf):

- LNA_IN (chip pin 1) → C28 1 pF to ground → R23 2.7 pF in series → C27
  (not fitted) to ground → **R22**.
- **R22** is a 0 Ω part drawn as a selector ("0R_NC") with three pads. One way
  it feeds **J5**, the ceramic chip antenna (CA-C03, marked "C3" on the left
  edge of the board). The other way it feeds **J4**, the U.FL / IPEX Gen 1
  (MHF1) connector.
- Factory setting: ceramic antenna. Waveshare: the IPEX connector is "enabled
  via resoldering an onboard resistor".

Where it is: Waveshare's board diagram puts the IPEX connector (#14) in the
top-left corner near the mounting hole and the ceramic antenna (#2) on the
left edge. In their photo, a short row of small parts runs from the chip
toward the antenna and ends in one part set at an angle; that is probably
R22. The U.FL socket is fitted on our board (confirmed 2026-11-02).

Our board under the microscope (2026-11-02) matches the schematic. From the
chip's side: C28 (fitted, shunt), R23 (fitted, series), C27 (two empty pads,
shunt), then R22 on a diagonal footprint with three pads. The incoming RF line
reaches the shared pad at the point of the "<". The resistor bridges it to
the outer pad whose trace runs to the ceramic antenna. The other outer pad is
empty; its trace runs to a via, presumably to the U.FL. To switch: move the
resistor so it bridges the shared pad and the empty pad (a quarter turn).
**Check with a meter first:** the empty pad should beep to the U.FL centre
pin, and the fitted outer pad to the ceramic antenna.

Doing it:

- The part is tiny (0402 or 0201). Use hot air or a fine iron tip. Turn the
  same resistor to bridge the other pair of pads, or fit a new 0 Ω.
- Only one path may be connected. Both connected splits the signal and both
  antennas work badly.
- Don't run the board with R22 on the IPEX side and no antenna attached:
  range drops to almost nothing and the base loses the shark.

## Brains: ESP32-S3-DEV-KIT-N16R8-U

Three ordered. Expected to use the same U.FL / IPEX Gen 1 connector as
Espressif's external-antenna modules usually do. **Not confirmed: check
when they arrive, and never transmit without an antenna attached.**

## Recommendation: patches facing each other along the pole

The base and the brains sit on the same stiff pole, so they stay lined up
whatever the pole does. Use a modest flat patch at each end: **the base's
faces straight up, the brains' face straight down.**

What it buys:

- **Base (the biggest win):** its main beam sees the shark and the sky, where
  almost no other devices are. Crowd devices off to the side arrive roughly
  10–20 dB weaker than through the ceramic antenna.
- **Brains (smaller win):** looking down, they see you and the few people
  right around the pole. The wider crowd is at shallow angles, outside the
  main beam.
- **Not immunity:** a phone a metre away is still loud whatever the antenna.
  At 2.5 m the problem is airtime, not weak signal: keeping other devices
  quieter at our receivers means our packets survive collisions more often and
  our transmitters wait less often for a clear channel. Choosing a quiet
  channel still matters as much as the antenna.

Gain and size:

- Aim for about **5–8 dBi with a 60–90° beam**. Higher gain means a bigger
  panel and a narrow beam that punishes misalignment and pole flex, and the
  extra signal isn't needed at 2.5 m.
- Legal: 20 dBm + 8 dBi = 28 dBm EIRP, under the FCC's 36 dBm limit for
  2.4 GHz (Part 15.247).

## Candidates

| Use | Antenna | Notes |
|---|---|---|
| Base | [E-Sonic/BPX 2867610](https://www.e-sonic.com/productfiles/mf-ph/2867610.pdf) | 8 dBi, 75° × 70° beam, 80 × 100 × 20 mm, SMA female, IP55 |
| Base | [L-com HyperGain 8 dBi flat patch](https://www.l-com.com/wireless-antenna-24-ghz-8-dbi-flat-patch-antenna-4ft-rp-sma-plug-connector) | 8 dBi, comes with a 4 ft RP-SMA cable; beam width not listed, check before buying |
| Base | [Interline 2.4 GHz panel](https://dev.linkshop.gr/wi-fi/antenna-2.4ghz/panels/interline-2.4ghz-panel-8dbi.html?sl=en) | 8 dBi, 60° × 60°, 115 × 115 × 18 mm (larger) |
| Brains (if room) | same panel as the base | matched pair is simplest |
| Brains (tight space) | [Taoglas WPC.25A.07.0150C](https://www.taoglas.com/datasheets/WPC.25A.07.0150C.pdf) | 25 × 25 mm ceramic patch, 150 mm cable to an IPEX MHF1 plug (fits the board directly), circularly polarized. Gain listed inconsistently: current datasheet 5 dBic, older revisions ([Mouser copy](https://www.mouser.com/datasheet/2/398/WPC.25A.07.0150C-1220953.pdf)) and distributors 0.5 dBi peak / −2.5 dBi average. Likely needs a ground plane behind it. The weaker option |

Two brains need two antennas, one each, both facing down.

## Polarization

- Circularly polarized patches (the WPC.25A, the
  [L-com CP patches](https://www.l-com.com/wireless-antenna-24-ghz-8-dbi-rh-circular-polarized-patch-antenna-4ft-rp-sma-plug-connector))
  need no orientation matching and reject some reflections, but **both ends
  must use the same handedness** (RH with RH).
- Linear with circular loses about 3 dB, harmless at this distance.
- Linear at both ends: mount them in the same orientation. Because both are on
  the same pole they stay matched as the pole turns.

## Mounting

- Put an **SMA bulkhead on each enclosure** with a short U.FL pigtail inside
  to the board. The U.FL stays plugged in and the antenna comes off for the
  checked bag. U.FL is only rated for about 30 matings and breaks easily under
  strain. A 10–15 cm pigtail loses about 0.5 dB, negligible.
- The aluminium pole runs alongside the signal path. That causes some
  reflections but no real loss at this range, so mounting the patches a few
  centimetres off the pole is fine.

## The other links

- The eyes keep their built-in omnidirectional antennas, so they stay the most
  exposed to interference. They sit inside the base patch's beam (helps) and
  within a metre of the brains (so the brains facing away from them doesn't
  matter).
- A phone or Mac on Nibbles works from where you stand at the base; people
  farther away connect poorly.

## Sources

- [Waveshare wiki: ESP32-S3-Touch-LCD-3.49](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49)
- [Waveshare docs: ESP32-S3-Touch-LCD-3.49](https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49)
- [3.49 schematic (PDF)](https://www.waveshare.net/w/upload/2/20/ESP32-S3-Touch-LCD-3.49-Schematic.pdf)
- [3.49 board resource diagram](https://docs.waveshare.net/assets/images/ESP32-S3-Touch-LCD-3.49-HW-d45f2a0617c5f4e05d497ae3533752b7.webp)
- [Waveshare ESP32-S3-LCD-1.9 (same resistor-switch approach)](https://www.waveshare.com/esp32-s3-lcd-1.9.htm?sku=30995)
- Antenna links in the table above.
