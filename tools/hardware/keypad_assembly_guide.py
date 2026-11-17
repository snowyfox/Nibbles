#!/usr/bin/env python3
"""Make the keypad boards' step-by-step placement pages.

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
        tools/hardware/keypad_assembly_guide.py

Reads each board in hardware/keypad/ (with KiCad's own Python, for pcbnew) and
writes hardware/keypad/assembly/<board>.html, using the power board's page
(assembly_guide.build). The step text is below; a step only lists the parts
its board has, and a step with none of them is left out. See
hardware/keypad/ASSEMBLY.md for the build as a whole.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assembly_guide as ag  # noqa: E402

OUT_DIR = os.path.join(ag.ROOT, "hardware", "keypad", "assembly")

PIN1 = dict(ag.PIN1)
PIN1["sk6812"] = ("pad 1 is GND: the LED's cut (chamfered) corner, and the leg with a notch in it, "
                  "go on the yellow dot")
PIN1["sot23-5"] = "pin 1: the dot on the chip; the side with 3 legs goes on the 3 pads"

# The key boards: every part is on the back (the switches press in from the
# front afterwards). Paste with the bottom stencil, place, hot plate; the LEDs
# go on by iron afterwards, onto the pads the paste has tinned.
KEY_SIDES = {
    "back": dict(title="Back: stencil, place, hot plate", face="bottom"),
    "iron": dict(title="Back: by iron, after the hot plate", face="bottom"),
}
KEY_STEPS = [
    dict(side="back", refs="U1", bag="C206010 / TCA9534APWR", pkg="TSSOP-16 (5 x 4.4 mm, 8 legs a side)",
         what="I/O expander: reads the keys and tells the base over I2C; pulls INT low when a key changes.",
         look="Thin black chip with 8 legs on each long side. Pin-1 dot or a dimple at one end.",
         pin1="ic", tip="The **A** part (C206010), not the power board's TCA9534PWR (C783615): the address range differs. "
                        "Legs 0.65 mm apart: check for bridges after the hot plate."),
    dict(side="back", refs="U3", bag="C7484 / SN74AHCT1G125DBVR", pkg="SOT-23-5 (3 legs one side, 2 the other)",
         what="LED data buffer: lifts the 3.3 V data from J1 pin 7 to 5 V for the first LED.",
         look="Small black part, 5 legs. Printed with a short code.",
         pin1="sot23-5", tip=None),
    dict(side="back", refs="RN1,RN2", bag="C29718 / 4D03WGJ0103T5E (10k x 4)", pkg="0603 x 4 resistor array (8 pads)",
         what="Pull-ups for the 8 key lines (the TCA9534A has none of its own).",
         look="Small black block, 4 pads on each long side, printed 103.",
         pin1=None, tip="Either way round: each resistor joins the same two pads both ways."),
    dict(side="back", refs="C1,C4,C11,C12,C13,C14,C15,C16,C17,C18", bag="C14663 / CC0603KRX7R9BB104 (100 nF)",
         pkg="0603 capacitor",
         what="Decoupling: C1 for U1, C4 for U3, C11 on are one per LED, beside it.",
         look="Small tan block, no marking.", pin1=None,
         tip="Capacitors aren't marked: open the bag only for this step."),
    dict(side="back", refs="C2", bag="C15850 / CL21A106KAYNNNE (10 uF 25 V)", pkg="0805 capacitor",
         what="Bulk capacitor on the 5 V for the LEDs.", look="Tan block, a little bigger than the 0603s, no marking.",
         pin1=None, tip=None),
    dict(side="back", refs="R1,R3", bag="C22775 / 0603WAF1000T5E (100 ohm)", pkg="0603 resistor",
         what="Series resistors on the LED data: R1 into the first LED (from U3), R3 from the last LED out to J2 pin 7.",
         look="Small black block, printed 1000 or 101.", pin1=None, tip=None),
    dict(side="back", refs="R2", bag="C25803 / 0603WAF1003T5E (100k)", pkg="0603 resistor",
         what="Pull-down on U3's input, so the LEDs stay dark with no data cable.",
         look="Small black block, printed 1003 or 104.", pin1=None, tip="Same size as R1/R3: keep the bags apart."),
    dict(side="back", refs="J1,J2", bag="C133062 / BM08B-GHS-TBT (JST-GH 8, vertical)", pkg="JST-GH 8-pin, vertical, SMD",
         what="J1: in (from the power board or the board before). J2: out to the next board.",
         look="White (or beige) 8-pin socket with two metal tabs at the ends.", pin1=None,
         tip="Fits one way: the 8 small pads and the two big tabs only line up one way round. Press it down flat after placing."),
    dict(side="back", refs="SW1,SW2,SW3,SW4,SW5,SW6,SW7,SW8", bag="C41430893 / Kailh CPG151101S11 hotswap socket",
         pkg="Hotswap socket, 14.5 x 5.9 mm, 2 metal tabs",
         what="Sockets for the Box Navy switches, which press into them from the front later.",
         look="Black (or white) plastic socket with two round bumps underneath and a metal tab at each end.",
         pin1=None, tip="The two bumps drop into the two holes, so it fits one way only. Place last, being the biggest: it shades nothing."),
    dict(side="iron", refs="D1,D2,D3,D4,D5,D6,D7,D8", bag="C5149201 / SK6812MINI-E", pkg="3.2 x 2.8 mm reverse-mount LED, 4 legs",
         what="The RGB light under each key. Mounted upside down: the body sits in the board's cutout, the lens looks out of the front.",
         look="White body with a clear lens on one face, two legs out of each side.",
         pin1="sk6812",
         tip="Lens **down**, into the hole from the back; the legs rest on the pads, which the hot plate has tinned. "
             "Flux, then touch each leg with the iron (about 300 C, 2-3 s a leg). Do one leg, check the LED sits flat and square, then the rest. "
             "The LEDs are moisture-sensitive: open the bag just before this step and seal it again after."),
    dict(side="iron", refs="JP1,JP2,JP3", bag="none: a sharp knife and a little solder", pkg="3-pad solder jumpers (A0, A1, A2)",
         what="The I2C address. As made, each jumper joins its middle pad to pad 1 (0), which gives 0x38.",
         look="Three small pads in a row, the middle one joined to one end by a thin trace.", pin1=None,
         tip="Only on the boards that need another address (see the table in ASSEMBLY.md): cut the thin trace between pads 1 and 2 "
             "with the knife, check with the meter that they no longer connect, then bridge pads 2 and 3 with solder."),
]

HUB_SIDES = {"top": dict(title="Top: stencil, place, hot plate", face="top"),
             "iron": dict(title="Top: by iron, after the hot plate", face="top")}
HUB_STEPS = [
    dict(side="top", refs="U1", bag="C7484 / SN74AHCT1G125DBVR", pkg="SOT-23-5 (3 legs one side, 2 the other)",
         what="LED data buffer: lifts the 3.49's 3.3 V LED data (GPIO 43) to 5 V for the key boards.",
         look="Small black part, 5 legs. Printed with a short code.", pin1="sot23-5", tip=None),
    dict(side="top", refs="C1,C4", bag="C14663 / CC0603KRX7R9BB104 (100 nF)", pkg="0603 capacitor",
         what="Decoupling for U1 (C1) and the 5 V (C4).", look="Small tan block, no marking.", pin1=None,
         tip="Capacitors aren't marked: open the bag only for this step."),
    dict(side="top", refs="R1", bag="C25803 / 0603WAF1003T5E (100k)", pkg="0603 resistor",
         what="Pull-down on U1's input, so the LEDs stay dark with nothing driving GPIO 43.",
         look="Small black block, printed 1003 or 104.", pin1=None, tip=None),
    dict(side="top", refs="R2", bag="C22775 / 0603WAF1000T5E (100 ohm)", pkg="0603 resistor",
         what="Series resistor on the buffered LED data.", look="Small black block, printed 1000 or 101.", pin1=None,
         tip="Right beside R1: keep the bags apart."),
    dict(side="top", refs="C2,C3", bag="C45783 / CL21A226MAQNNNE (22 uF)", pkg="0805 capacitor",
         what="Bulk capacitors on the 5 V for the key boards' LEDs.", look="Tan block, no marking.", pin1=None, tip=None),
    dict(side="top", refs="J1,J2,J3,J4,J5", bag="C133062 / BM08B-GHS-TBT (JST-GH 8, vertical)", pkg="JST-GH 8-pin, vertical, SMD",
         what="J1: from the power board (J9 plus the GPIO 43 wire). J2-J5: out to key boards 1-4.",
         look="White (or beige) 8-pin socket with two metal tabs at the ends.", pin1=None,
         tip="Fits one way. J4 and J5 face the other way from J1-J3 (the board has them turned round)."),
    dict(side="iron", refs="JP1,JP2,JP3,JP4", bag="none: a little solder", pkg="2-pad solder jumpers",
         what="Skip a port: the LED data runs out through each key board and back, port by port. "
              "JP1 skips J2, JP2 J3, JP3 J4, JP4 J5. Open as made.",
         look="Two small pads side by side.", pin1=None,
         tip="Bridge (a blob of solder across both pads) every port with nothing plugged in, or the LEDs after it stay dark. "
             "A chain-version key board on the hub sends nothing back, so it must be on the last port in use."),
]

BOARDS = [
    dict(name="key_column_chain", steps=KEY_STEPS, sides=KEY_SIDES, landmarks=["J1", "J2"],
         title="Key Column Placement", heading="Key column (chain): placement",
         intro="8 keys, J1 in, J2 out. One step per bag. Click a step (or use ← →); its parts light up on the map. "
               "Tick it when placed. Map: the back, as you see it with the board turned over side to side; "
               "key 1 at the top, the connectors at the bottom."),
    dict(name="key_upper", steps=KEY_STEPS, sides=KEY_SIDES, landmarks=["J1", "J2"],
         title="Upper Keypad Placement", heading="Upper keypad (chain, 3 keys): placement",
         intro="3 keys, J1 in, J2 out. One step per bag. Click a step (or use ← →); its parts light up on the map. "
               "Tick it when placed. Map: the back, as you see it with the board turned over side to side; "
               "key 1 at the top, the connectors at the bottom."),
    dict(name="key_column", steps=KEY_STEPS, sides=KEY_SIDES, landmarks=["J1"],
         title="Hub Column Placement", heading="Key column (hub version): placement",
         intro="8 keys, one connector (J1) to a hub port. One step per bag. Click a step (or use ← →); its parts "
               "light up on the map. Tick it when placed. Map: the back, as you see it with the board turned over "
               "side to side; key 1 at the top, the connector at the bottom."),
    dict(name="key_hub", steps=HUB_STEPS, sides=HUB_SIDES, landmarks=["J1", "J2", "J3", "J4", "J5"],
         title="Keypad Hub Placement", heading="Keypad hub: placement",
         intro="Everything on the top. One step per bag. Click a step (or use ← →); its parts light up on the map. "
               "Tick it when placed. Map: the top as you look down on it, J1 (from the power board) top left."),
]


def main():
    import pcbnew
    for b in BOARDS:
        pcb = os.path.join(ag.ROOT, "hardware", "keypad", b["name"], b["name"] + ".kicad_pcb")
        refs = {fp.GetReference() for fp in pcbnew.LoadBoard(pcb).GetFootprints()}
        steps = []
        for s in b["steps"]:
            have = [r for r in s["refs"].split(",") if r in refs]
            if have:
                steps.append(dict(s, refs=",".join(have)))
        meta = dict(title=b["title"], heading=b["heading"], intro=b["intro"],
                    key="nibbles-" + b["name"].replace("_", "-") + "-placement", landmarks=b["landmarks"])
        ag.build(pcb, os.path.join(OUT_DIR, b["name"] + ".html"), steps, b["sides"], meta, pin1=PIN1)


if __name__ == "__main__":
    main()
