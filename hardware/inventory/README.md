# Parts inventory

`inventory.csv` is the record of parts bought, for this project and later
ones. Edit it by hand or in a spreadsheet. It's a snapshot, so
`tools/hardware/combined_bom.py` doesn't touch it.

- **Ordered / Received:** how many were bought and how many have arrived.
  Received stays 0 until the order is checked in; then set it and change
  Status to `In stock`.
- **Reserved for Nibbles:** what 5 of every board use (power + charger, key
  column, upper keypad, hub). Lower it if fewer get built.
- **Free:** Ordered minus Reserved. Treat a part as usable only once it has
  been received. Lower it when parts go into another project.

Order log:

| Date | Supplier | What |
|---|---|---|
| 2026-11-01 | LCSC | Everything in `hardware/fab/combined-LCSC-order.csv` as of that day (58 lines, about $242 before shipping). Not the optional cable parts. |
| 2026-11-02 | JLCPCB | The bare PCBs (power + charger, 4-layer, with stencil): ordered 2026-10-28 (Wed), shipped 2026-11-02 (Mon), DHL tracking 9816452192, delivered 2026-11-04 (Wed). About 7 days door to door: 5 to make, 2 in DHL. |
| 2026-11-04 | Amazon | Solder paste: Chip Quik NC191AX50 (Sn63/Pb37) and SMDLTLFPT4 (low-temp Sn42/Bi57.6/Ag0.4), one of each. |
| 2026-11-04 | Amazon | NTC kit (5 x 10k B3435 50 mm, 5 x 100k B3950 60 mm; the 10k ones replace the 103AT-2 on J4) and a TVS kit with 10 x SMBJ22A (D2 for a 20 V dongle). |
