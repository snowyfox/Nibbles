#!/bin/sh
# Put this Mac's Wi-Fi on the shark's network for updates, and back again.
#
#   tools/nibbles_wifi.sh join [PASSWORD]   join "Nibbles" (default password wled1234, WLED's)
#   tools/nibbles_wifi.sh leave             forget it and rejoin the usual network
#
# `networksetup -setairportnetwork` fails with this access point (-3912);
# CoreWLAN joins it. The brain's access point takes only a few clients: if the
# join fails with "tmpErr", send boards off Wi-Fi first ("ota remote off all").
# With Ethernet ahead of Wi-Fi in the service order the Mac keeps its internet.
set -e
case "$1" in
join)
    pass="${2:-wled1234}"
    cat > /tmp/nibbles_join.swift <<'EOF'
import CoreWLAN
let i = CWWiFiClient.shared().interface()!
// A full scan, picked by name: scanForNetworks(withName:) found nothing at
// a hotel while a full scan listed Nibbles at -28 dBm (2026-11-21). Try a few times.
var found: CWNetwork? = nil
// Scans come back incomplete now and then (32 of 79 networks), so try the
// system's cached results and fresh scans, up to ten times.
func pick(_ s: Set<CWNetwork>?) -> CWNetwork? {
    (s ?? []).filter { $0.ssid == "Nibbles" }.max { $0.rssiValue < $1.rssiValue }
}
for _ in 0..<10 {
    found = pick(i.cachedScanResults()) ?? pick(try? i.scanForNetworks(withSSID: nil))
    if found != nil { break }
    Thread.sleep(forTimeInterval: 3)
}
guard let n = found else { print("Nibbles not found"); exit(1) }
do { try i.associate(to: n, password: CommandLine.arguments[1]); print("joined Nibbles") }
catch { print("could not join:", error); exit(1) }
EOF
    swift /tmp/nibbles_join.swift "$pass"
    sleep 4
    echo "this Mac: $(ipconfig getifaddr en0)"
    ;;
leave)
    networksetup -removepreferredwirelessnetwork en0 Nibbles >/dev/null 2>&1 || true
    networksetup -setairportpower en0 off
    sleep 2
    networksetup -setairportpower en0 on
    sleep 10
    echo "this Mac: $(ipconfig getifaddr en0)"
    ;;
*)
    sed -n '2,10p' "$0"
    exit 2
    ;;
esac
