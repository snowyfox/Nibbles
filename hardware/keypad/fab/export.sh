#!/bin/zsh
# Rebuild the JLCPCB files for one keypad board: ./export.sh key_upper (or key_column, key_hub).
# Close the board in KiCad first. BOARDS=n sets how many boards the LCSC order list is for (default 2).
# Works on a copy, so the project files are never touched; zones are refilled on the copy.
set -e
here=${0:A:h}
name=${1:?usage: export.sh key_upper|key_column|key_hub}
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
work=$(mktemp -d)
mkdir $work/$name
cp -R $here/../nibbles_keys.pretty $work/
cp $here/../$name/fp-lib-table $here/../$name/$name.kicad_{pro,pcb,sch} $work/$name/
cd $work/$name
$KPY -c "import pcbnew; b = pcbnew.LoadBoard('$name.kicad_pcb'); pcbnew.ZONE_FILLER(b).Fill(b.Zones()); pcbnew.SaveBoard('$name.kicad_pcb', b)" 2>/dev/null

# checks: stop if anything electrical is wrong
$K sch erc --severity-error --exit-code-violations -o erc.rpt $name.kicad_sch >/dev/null
$K pcb drc --schematic-parity --severity-error --exit-code-violations -o drc.rpt $name.kicad_pcb >/dev/null

# Gerbers (Protel extensions, soldermask subtracted from silk) and Excellon drills (mm, decimal, PTH/NPTH apart)
mkdir gerbers
$K pcb export gerbers -o gerbers --layers "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" \
    --subtract-soldermask --no-x2 --no-netlist --precision 6 $name.kicad_pcb >/dev/null
$K pcb export drill -o gerbers/ --format excellon --drill-origin absolute --excellon-zeros-format decimal \
    --excellon-oval-format alternate --excellon-units mm --excellon-separate-th $name.kicad_pcb >/dev/null
rm -f gerbers/*.gbrjob
mkdir -p $here/$name
rm -f $here/$name/$name-gerbers.zip
zip -q -j $here/$name/$name-gerbers.zip gerbers/*

# BOM and placement in JLCPCB's column formats; parts without an LCSC number (holes, solder jumpers) aren't placed parts
$K sch export bom -o bom.csv --fields 'Value,Reference,Footprint,${QUANTITY},LCSC' --labels 'Comment,Designator,Footprint,Qty,LCSC' \
    --group-by 'Value,Footprint,LCSC' --ref-range-delimiter '' --exclude-dnp $name.kicad_sch >/dev/null
$K pcb export pos -o pos.csv --format csv --units mm --side both --exclude-dnp $name.kicad_pcb >/dev/null
python3 - $here/$name $name ${BOARDS:-2} <<'PY'
import csv, sys
out, name, boards = sys.argv[1], sys.argv[2], int(sys.argv[3])
rows = [r for r in csv.DictReader(open("bom.csv")) if r["LCSC"].strip()]
# the switch symbols carry the hotswap socket's part number: the socket is what gets soldered, on the back
SOCKET = "C41430893"
for r in rows:
    if r["LCSC"].strip() == SOCKET: r["Comment"] = "Kailh hotswap socket CPG151101S11"
merged, order = {}, []
for r in rows:   # JLCPCB wants one line per part number
    key = r["LCSC"].strip()
    if key not in merged: merged[key] = [r["Comment"], [], r["Footprint"].split(":")[-1], key]; order.append(key)
    merged[key][1] += [d.strip() for d in r["Designator"].split(",") if d.strip()]
num = lambda d: (d.rstrip("0123456789"), int(d[len(d.rstrip("0123456789")):] or 0))
with open(f"{out}/{name}-BOM-JLCPCB.csv", "w", newline="") as f, open(f"{out}/{name}-LCSC-order.csv", "w", newline="") as g:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    wg = csv.writer(g); wg.writerow(["LCSC Part Number", "Quantity", "Comment", "Designator"])
    for k in order:
        c, refs, fp, lcsc = merged[k]; refs.sort(key=num)
        w.writerow([c, ",".join(refs), fp, lcsc]); wg.writerow([lcsc, len(refs) * boards, c, ",".join(refs)])
refs = {d for k in order for d in merged[k][1]}
sockets = set(merged.get(SOCKET, [None, []])[1])
with open(f"{out}/{name}-CPL-JLCPCB.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for p in csv.DictReader(open("pos.csv")):
        if p["Ref"] in refs:
            w.writerow([p["Ref"], p["PosX"] + "mm", p["PosY"] + "mm", "Top" if p["Side"] == "top" and p["Ref"] not in sockets else "Bottom", p["Rot"]])
PY
rm -rf $work
echo "Wrote $here/$name/: $name-gerbers.zip, -BOM-JLCPCB.csv, -CPL-JLCPCB.csv, -LCSC-order.csv (${BOARDS:-2} boards)"
