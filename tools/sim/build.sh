#!/bin/sh
# Builds web/sim.js + web/sim.wasm with Emscripten (emcc on PATH, e.g. after
# `source <emsdk>/emsdk_env.sh`).
set -e
cd "$(dirname "$0")"
# The eyes: the eye firmware's own analysis, behaviour and drawing code.
EYES=../../firmware/eyes/main
LINK=../../shared/nibbles_link
emcc -O2 src/ar_sim.c src/sim_wasm.c src/eyes_wasm.c \
  $EYES/audio_analysis.c $EYES/eye.c $EYES/presets.c $EYES/render_core.c $LINK/src/nl_ar.c \
  -I$EYES -I$LINK/include \
  -s MODULARIZE=1 -s EXPORT_NAME=SimModule -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS='["HEAPF32","HEAPU8","HEAPU32","UTF8ToString"]' -s ALLOW_MEMORY_GROWTH=1 \
  -o web/sim.js
echo "built web/sim.js and web/sim.wasm"
