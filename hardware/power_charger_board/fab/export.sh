#!/bin/zsh
# Rebuild the JLCPCB files in this folder from the KiCad project (close the board in KiCad first).
# BOARDS=n sets how many boards the LCSC order list is for (default 2).
# Works on a copy, so the project files are never touched; zones are refilled on the copy.
set -e
here=${0:A:h}
proj=$here/../kicad
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
work=$(mktemp -d)
cp -R $proj/nibbles.pretty $proj/nibbles_ti.pretty $proj/nibbles.kicad_sym $proj/fp-lib-table $proj/sym-lib-table \
      $proj/nibbles_power_charger.kicad_{pro,pcb,sch} $work/
cd $work
$KPY -c "import pcbnew; b = pcbnew.LoadBoard('nibbles_power_charger.kicad_pcb'); pcbnew.ZONE_FILLER(b).Fill(b.Zones()); pcbnew.SaveBoard('nibbles_power_charger.kicad_pcb', b)" 2>/dev/null

# checks: stop if anything electrical is wrong
$K sch erc --severity-error --exit-code-violations -o erc.rpt nibbles_power_charger.kicad_sch >/dev/null
$K pcb drc --schematic-parity --severity-error --exit-code-violations -o drc.rpt nibbles_power_charger.kicad_pcb >/dev/null

# Gerbers (Protel extensions, soldermask subtracted from silk) and Excellon drills (mm, decimal, PTH/NPTH apart)
mkdir gerbers
$K pcb export gerbers -o gerbers --layers "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" \
    --subtract-soldermask --no-x2 --no-netlist --precision 6 nibbles_power_charger.kicad_pcb >/dev/null
$K pcb export drill -o gerbers/ --format excellon --drill-origin absolute --excellon-zeros-format decimal \
    --excellon-oval-format alternate --excellon-units mm --excellon-separate-th nibbles_power_charger.kicad_pcb >/dev/null
rm -f gerbers/*.gbrjob
rm -f $here/nibbles_power_charger-gerbers.zip
zip -q -j $here/nibbles_power_charger-gerbers.zip gerbers/*

# BOM and placement in JLCPCB's column formats
$K sch export bom -o bom.csv --fields 'Value,Reference,Footprint,${QUANTITY},LCSC' --labels 'Comment,Designator,Footprint,Qty,LCSC' \
    --group-by 'Value,Footprint,LCSC' --ref-range-delimiter '' --exclude-dnp nibbles_power_charger.kicad_sch >/dev/null
$K pcb export pos -o pos.csv --format csv --units mm --side both --exclude-dnp nibbles_power_charger.kicad_pcb >/dev/null
python3 - $here ${BOARDS:-2} <<'EOF'
import csv, sys
out = sys.argv[1]
rows = list(csv.DictReader(open("bom.csv")))
# JLCPCB wants one line per part number: merge lines that share one (e.g. "0.1uF" and "100nF")
merged, order = {}, []
for r in rows:
    key = r["LCSC"].strip() or ("value", r["Comment"], r["Footprint"])
    if key not in merged: merged[key] = [r["Comment"], [], r["Footprint"].split(":")[-1], r["LCSC"].strip()]; order.append(key)
    merged[key][1] += [d.strip() for d in r["Designator"].split(",") if d.strip()]
with open(f"{out}/nibbles_power_charger-BOM-JLCPCB.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for k in order:
        c, refs, fp, lcsc = merged[k]
        refs.sort(key=lambda d: (d.rstrip("0123456789"), int(d[len(d.rstrip("0123456789")):] or 0)))
        w.writerow([c, ",".join(refs), fp, lcsc])
# LCSC order for building by hand: every line x BOARDS, plus trial values for the eFuse timing caps (C35-C37),
# to swap after bench tests. LCSC rounds small passives up to its minimum order anyway.
boards = int(sys.argv[2])
extra = {"C1589": ("10nF 50V 0603", "trial: ITIMER 4.7 ms"), "C21122": ("22nF 50V 0603", "trial: dVdt 80 ms ramp"),
         "C1622": ("47nF 50V 0603", "trial: ITIMER 22 ms, RETRY_DLY 2.2 s"), "C14663": ("100nF 50V 0603", "trial: dVdt 365 ms, RETRY_DLY 4.7 s")}
buy = []   # (LCSC number, quantity, comment, designators)
for k in order:
    c, refs, fp, lcsc = merged[k]
    n = len(refs) * boards * (2 if lcsc == "C2932674" else 1) + (10 if lcsc in extra else 0)   # J6 is two 1x11 headers
    buy.append((lcsc, n, c + (" + " + extra[lcsc][1] if lcsc in extra else ""), ",".join(refs)))
buy += [(lcsc, 10, c + " " + why, "") for lcsc, (c, why) in extra.items() if lcsc not in merged]
with open(f"{out}/nibbles_power_charger-LCSC-order.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["LCSC Part Number", "Quantity", "Comment", "Designator"]); w.writerows(buy)
# DigiKey order (digikey_map.csv: the DigiKey part for each LCSC number), and the lines DigiKey can't supply, from LCSC
dk = {m["LCSC"]: m for m in csv.DictReader(open(f"{out}/digikey_map.csv"))}
with open(f"{out}/nibbles_power_charger-DigiKey-order.csv", "w", newline="") as f, \
     open(f"{out}/nibbles_power_charger-LCSC-DigiKey-gaps.csv", "w", newline="") as g:
    w = csv.writer(f); w.writerow(["Manufacturer Part Number", "Manufacturer", "Quantity", "Customer Reference", "Description", "Note"])
    wg = csv.writer(g); wg.writerow(["LCSC Part Number", "Quantity", "Comment", "Designator"])
    for lcsc, n, c, refs in buy:
        m = dk.get(lcsc)
        if m and m["Manufacturer Part Number"]: w.writerow([m["Manufacturer Part Number"], m["Manufacturer"], n, refs, c, m["Note"]])
        else: wg.writerow([lcsc, n, c + (" (" + m["Note"] + ")" if m else " (not in digikey_map.csv yet)"), refs])
refs = {d.strip() for r in rows for d in r["Designator"].split(",")}
with open(f"{out}/nibbles_power_charger-CPL-JLCPCB.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for p in csv.DictReader(open("pos.csv")):
        if p["Ref"] in refs:   # test points, holes and the logo aren't placed parts
            w.writerow([p["Ref"], p["PosX"] + "mm", p["PosY"] + "mm", "Top" if p["Side"] == "top" else "Bottom", p["Rot"]])
EOF
rm -rf $work
echo "Wrote $here/nibbles_power_charger-gerbers.zip, -BOM-JLCPCB.csv, -CPL-JLCPCB.csv, -LCSC-order.csv, -DigiKey-order.csv + -LCSC-DigiKey-gaps.csv (${BOARDS:-2} boards)"
