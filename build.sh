#!/bin/sh
# .murkk build — one translation unit, size-optimized, stripped
set -e
gcc -Os -Wall \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -ffunction-sections -fdata-sections \
  murkk.c -o murkk \
  -Wl,--gc-sections -lSDL2 -lGL -lm
strip -s murkk
SZ=$(stat -c%s murkk)
echo "murkk: ${SZ} bytes ($(( SZ / 1024 )) KB) — budget 98304 (96 KB)"
[ "$SZ" -le 98304 ] && echo "UNDER BUDGET" || echo "OVER BUDGET"
