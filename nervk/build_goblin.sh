#!/bin/sh
# .nervk armory dlopen tiny build: compression-aware goblin variant.
# SDL/GL/libm are resolved at runtime; only libc remains as DT_NEEDED.
#
# Default is FORCE_COMPAT=1 because the bundled minimal SDL/GL headers
# produce smaller/more reproducible code than distro headers on some boxes.
# Set USE_SYSTEM_HEADERS=1 if you explicitly want pkg-config/system headers.
set -eu

SRC=${1:-nervk_armory_dlopen.c}
OUT=${OUT:-nervk_armory_dlopen}
RAW=${RAW:-$OUT.raw}
SST=${SST:-$OUT.sstrip}
TIN=${TIN:-$OUT.xzrun}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

CFLAGS="-Oz -Wall -flto -fwhole-program -fno-plt -no-pie -fno-toplevel-reorder -fno-reorder-functions -fno-schedule-insns -fno-schedule-insns2 -fno-ipa-cp -fno-ipa-sra -fno-tree-sra -fno-expensive-optimizations -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -fno-math-errno -fno-ident"
LDFLAGS="-flto -no-pie -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-z,noseparate-code -Wl,--as-needed -Wl,--hash-style=sysv"

if [ "${USE_SYSTEM_HEADERS:-0}" = 1 ]; then
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 && [ -e /usr/include/GL/gl.h ]; then
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
    printf '[build] headers:   system SDL2/GL\n'
  else
    SDL_CFLAGS="-I$HERE/compat"
    printf '[build] headers:   bundled compat fallback\n'
  fi
else
  SDL_CFLAGS="-I$HERE/compat"
  printf '[build] headers:   bundled compat\n'
fi

printf '[build] compiling %s\n' "$SRC"
gcc $CFLAGS $SDL_CFLAGS "$SRC" -o "$RAW" $LDFLAGS
strip -s "$RAW"
RAW_SZ=$(stat -c%s "$RAW")
printf '[build] strip:     %s bytes\n' "$RAW_SZ"

cp "$RAW" "$SST"
if command -v sstrip >/dev/null 2>&1; then
  sstrip "$SST"
else
  [ -f "$HERE/tiny_tools/sstrip64.py" ] || { echo "[build] missing tiny_tools/sstrip64.py" >&2; exit 1; }
  python3 "$HERE/tiny_tools/sstrip64.py" "$SST"
fi
SST_SZ=$(stat -c%s "$SST")
printf '[build] sstrip:    %s bytes\n' "$SST_SZ"

# Autotune xz literal-context/position-bits. Different compiler/xz versions
# move the winning setting around by a few hundred bytes.
best=""
best_sz=999999999
best_desc=""
for lc in 0 1 2 3 4; do
  for pb in 0 1 2; do
    tmp="$SST.xz.lc${lc}.pb${pb}"
    xz -9e --check=none --lzma2=preset=9e,pb=$pb,lc=$lc -c "$SST" > "$tmp"
    sz=$(stat -c%s "$tmp")
    printf '[build] xz try:    lc=%s pb=%s => %s bytes\n' "$lc" "$pb" "$sz"
    if [ "$sz" -lt "$best_sz" ]; then
      best_sz=$sz
      best=$tmp
      best_desc="lc=$lc pb=$pb"
    fi
  done
done
mv "$best" "$SST.xz"
rm -f "$SST".xz.lc*.pb* 2>/dev/null || true
XZ_SZ=$(stat -c%s "$SST.xz")
printf '[build] xz best:   %s bytes (%s)\n' "$XZ_SZ" "$best_desc"

cat > "$TIN" <<'STUB'
#!/bin/sh
t=/tmp/n.$$;tail -n+3 $0|xz -dc>$t&&chmod +x $t&&$t "$@";r=$?;rm -f $t;exit $r
STUB
cat "$SST.xz" >> "$TIN"
chmod +x "$TIN"
TIN_SZ=$(stat -c%s "$TIN")
printf '[build] runner:    %s bytes (%s KiB)\n' "$TIN_SZ" "$((TIN_SZ / 1024))"
[ "$TIN_SZ" -le 98304 ] && echo '[build] budget:    UNDER 96 KiB' || echo '[build] budget:    OVER 96 KiB'
[ "$TIN_SZ" -le 32768 ] && echo '[build] budget:    UNDER 32 KiB' || echo '[build] budget:    OVER 32 KiB'
printf '[build] DT_NEEDED:\n'
readelf -d "$RAW" | grep NEEDED || true
