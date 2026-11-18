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
guard let n = try? i.scanForNetworks(withName: "Nibbles").first else { print("Nibbles not found"); exit(1) }
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
