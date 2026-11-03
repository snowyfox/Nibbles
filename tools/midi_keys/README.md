# Base keys from a MIDI keyboard

Plays the real base station's keypad from a MIDI keyboard (an M-Audio
KeyStudio here), for testing before the keypads exist. No screen: a status
page shows whether the keyboard and the base are connected, a picture of the
keyboard with each key's mapping, every base key with the MIDI keys mapped to
it, and lets you remap them. The volume slider sets WLED's brightness.

```
python3 tools/midi_keys/server.py        # then open http://localhost:8090/ in Chrome
```

Chrome asks once to allow MIDI. The tab must stay open (it can sit in the
background): the page reads the keyboard through Web MIDI, which Safari
doesn't have.

## How it works

- **The page** (`web/index.html`) reads the keyboard. A key press on a mapped
  key sends that base key down, its release sends it up, so holding works
  (bumps, TAKE, holding a number and tapping LATCH, holding UNLOCK). Requests
  go one at a time so downs and ups arrive in order.
- **`server.py`** (Python standard library only) writes `key NAME down|up` to
  the base's USB serial console, the same command the simulator's "real base"
  mode uses (see `CLAUDE.md`). It finds the base by its USB serial number
  (`BASE_SERIAL`, its MAC) through `ioreg`, and again after a restart or a
  replug; or give the port with `--base /dev/cu.usbmodemNNN`. Opening the
  port can restart the base: give it ~15 s to find the radio channel.
- The key names come from the firmware (`key_names` in
  `firmware/base/main/controls.c`), so a new key appears on the page by itself.
- **Brightness:** the volume slider (CC 7) sends `wled bri N` (0 at the
  bottom, which turns WLED off, to 255 at the top), at most every 80 ms while
  it moves. The KeyStudio's slider is noisy (untouched, it wanders about 5
  steps), so the page only follows it once it leaves a band of 4 steps
  either side of where it last settled (`SLIDER_H`), with the range stretched
  so the ends still reach 0 and 255. An untouched slider changes nothing:
  move it to take over. It also gives single wild readings (255, 145, 255
  at rest): a reading more than 20 steps from the last (`SLIDER_JUMP`) is
  only used if the next one agrees. Sends during a move are paced by the
  slider's own messages; a timer only sends where it stopped, which Chrome
  may delay up to about a second when the tab is hidden. The base sends it to the Live brain; the Standby brain follows
  within a second (the base's Standby sync). The base status card shows the
  brightness WLED reports. Other sliders or the mod wheel can be added to
  BRIGHTNESS with **Add slider**.
- **The keyboard picture** (the KeyStudio 49 at its centre octave, C2 to C6)
  labels every white key with its note and every mapped key with its base
  key. Click a row to light its keys (and the pedal or slider) in purple;
  click a key in the picture to find its row. Keys being pressed light up
  yellow, so it also shows which note you're on.
- The page shows each key's LED colour as the base lights it (`leds on`) and
  the end of the base's log, where every key shows as `key NAME down|up`.
- Closing the page, a base that goes away, or **Release all keys** lets go of
  every key the bridge holds down.

## Mapping

- **Every base key has a row**, in sections: numbers 1-16, the modifiers
  (PALETTE, LAYER, BRIGHT, PAGE), targets, the switcher keys, then keys of
  their own for each preset (PRESET1-16), palette (PALETTE1-8), layer look
  (LAYER1-16) and brightness step (BRIGHT1-8), and POLEPOWER. None has to be
  mapped. A number does what the base's held modifier says (LAYER held + 3:
  layer look 3); a key of its own does its one thing whatever is held.
- **Combinations**, like hotkeys: a row can be mapped to a MIDI key pressed
  while modifiers are held, e.g. LAYER3 on "LAYER + E4". To record one, click
  **Add key**, hold the modifier's MIDI key (it must be mapped to the
  modifier already) and press the other key; tapping the modifier's key alone
  maps that key itself. The same MIDI key can have its plain mapping and
  combinations: with the modifiers held, the combination wins (the one with
  the most modifiers), else the plain one, so E4 alone can stay number 3.
  The modifier itself still goes to the base, so a MIDI key with no
  combination does what the base makes of it (LAYER + number 3: layer look
  3). A key let go of releases what it pressed, even if the modifier went
  first. Holding a modifier relabels the keyboard picture with what each
  key does with it. Saved as `"layer+note:64"` in the button's list.

- `mapping.default.json` is the default; your changes save to `mapping.json`
  (not committed). **Reset to default mapping** goes back.
- **Add key**, then press a piano key (or the sustain pedal) to map it; Esc
  cancels. A MIDI key maps to one base key (mapping it again moves it). A
  base key can have several MIDI keys: by default LATCH is on A3 and the
  sustain pedal, and stays down while either is.
- Notes are numbered as MIDI notes (60 = middle C, shown as C4). The
  KeyStudio's octave buttons shift the notes it sends, so leave it at the
  centre octave (or remap).
- Sustain pedal: down at value 64 and above. A pedal with the opposite
  polarity reads backwards (plug it in before powering the keyboard and most
  keyboards detect that).

Default layout (left hand targets and the switcher keys, right hand numbers):

| MIDI | Key | | MIDI | Key |
|---|---|---|---|---|
| C3 (48) | ALL | | C4 (60) … C5 (72), white keys | 1 … 8 |
| D3 (50) | BODY | | D5 (74) | FLASH |
| E3 (52) | EYES | | E5 (76) | BLACKOUT |
| F3 (53) | FINS | | F5 (77) | FINSENSOR |
| G3 (55) | PAGE | | G5 (79) | UNLOCK |
| A3 (57), sustain pedal | LATCH | | | |
| B3 (59) | TAKE | | | |
