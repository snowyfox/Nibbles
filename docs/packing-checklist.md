# Packing checklist (festival 2026-11-20 and 21)

Flight: Southwest, evening of 2026-11-18, to Long Beach. Draft from what the
repo knows (2026-11-09); add the things only you know about. Battery rules
are in `docs/power.md`, "Travel notes".

## Before packing (by about 2026-11-17)

- [ ] Final firmware flashed and its commit pushed: eyes (both), base,
      Right Brain, Left Brain, controller usermod, preview screen.
- [ ] `KEYPAD_BOARDS` in `firmware/base/main/config.h` matches the keypads
      actually fitted.
- [ ] Backups of every board's flash and WLED's `cfg.json` / `presets.json`
      on the laptop (and somewhere off it).
- [ ] Full-system test on the pack, then docked: lights, eyes, A/B take,
      keypads, preview, base screen, log on the TF card.
- [ ] 24-pin shark connector: continuity check, pin by pin, against the
      table in `wled/README.md` (8 colours repeat: white, brown, green, blue,
      red, yellow, black, orange). Same for the LEMO cable (pin 1 white to
      pin 8 orange).
- [ ] Shark cable ground: continuous and making contact (see the USB ground
      rule below).
- [ ] Enclosures closed and sealed: top box, eye domes, base.
- [ ] TF card in the base 3.49.

## Ship ahead (to the friend in Long Beach)

- [ ] External batteries, or buy / rent them there (USPS: surface only,
      original sealed packaging; used VB99s don't clearly qualify). Don't
      ship the home-built internal pack.
- [ ] Shipment tracked and the friend knows it's coming.

## Checked bag

- [ ] Pole (K-Tek K-123) and its sections.
- [ ] Shark, with eyes and fins, padded.
- [ ] Top box (controller, Right and Left Brains), if it doesn't travel on the shark.
- [ ] Dock / backpack enclosure (no batteries in it).
- [ ] Cables: LEMO 1K.308 dock cable, eye cable, spare JST-GH keypad cables.
- [ ] No lithium batteries of any kind in this bag.

## Carry-on

- [ ] Base, with the internal pack (label its Wh; ~90 Wh).
- [ ] At most one other lithium pack in the cabin, under 100 Wh, terminals
      covered (Southwest: one power bank per person; check before flying).
- [ ] Keypads.
- [ ] Laptop and charger.
- [ ] Spare boards: spare 3.49 (first base prototype, 28:84:85:91:27:08),
      spare eye if there is one.

## Tools and spares

- [ ] USB-C cables (data, not charge-only) for flashing; a USB hub.
- [ ] ESP-IDF and the WLED build tools working on the laptop, offline
      (build once without Wi-Fi to check).
- [ ] USB-C to LEMO charging dongle and a USB-C PD charger (100 W).
- [ ] Multimeter, small screwdrivers, flush cutters, electrical tape,
      heat shrink, zip ties, cable ties for the pole.
- [ ] Soldering iron (travel / battery), solder, flux.
- [ ] Spare JST-GH and Dupont leads, a few 2020 LEDs or a short strip.
- [ ] Spare fuses / the 20 A switch's spare, if any.
- [ ] Gaffer tape.

## Rules to remember

- **No shark-side USB when plugging the shark into the power board.** Unplug
  the controller's and brains' USB first; a ground loop killed a 3.49 on
  2026-11-08.
- After powering up, open `http://<WLED>/time` on a phone once so the base's
  clock (and its charge graph) is set: the final build has no clock battery.
- If the base boots before the brains it may sit on the wrong channel: power
  the shark first, or restart the base.

## Each day at the hotel

- [ ] Charge the internal pack (dongle) and every external battery.
- [ ] Copy the day's logs off the base (`tools/base_log.py`).
- [ ] Look over the log: pack temperature, faults, dropped rows.

## Each night before going out

- [ ] Batteries charged, dock packed.
- [ ] Power-up check: lights, both eyes, keypads, preview, base screen.
- [ ] Pole sections locked, shark mount tight.
