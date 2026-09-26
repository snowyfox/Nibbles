#!/bin/sh
# Opens the simulator in its own Chrome window with Chrome's software GPU
# (SwiftShader) turned on. That gives the full 3D view on Macs whose graphics
# drivers don't support WebGL. It uses a separate Chrome profile, so it runs
# alongside your normal Chrome windows.
#   tools/sim/open_chrome.sh [url]      (default http://localhost:8080/)
URL=${1:-http://localhost:8080/}
PROFILE="$HOME/Library/Application Support/NibblesSimChrome"
exec "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  --user-data-dir="$PROFILE" --use-angle=swiftshader --enable-unsafe-swiftshader \
  --ignore-gpu-blocklist --no-first-run --new-window "$URL" >/dev/null 2>&1 &
