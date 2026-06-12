#!/bin/sh
# .nervk armory tiny build: strip + sstrip + vondehi-style xz runner.
# No -nostartfiles, no shader minification.
set -eu

SRC=${1:-nervk_armory.c}
OUT=${OUT:-nervk_armory}
RAW=${RAW:-$OUT.raw}
SST=${SST:-$OUT.sstrip}
TIN=${TIN:-$OUT.xzrun}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

CFLAGS="-Os -Wall -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -fno-math-errno -fno-ident"
LDFLAGS="-Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,--as-needed -Wl,--hash-style=gnu"

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 && [ -e /usr/include/GL/gl.h ]; then
  SDL_CFLAGS=$(pkg-config --cflags sdl2)
  SDL_LIBS=$(pkg-config --libs sdl2)
  GL_LIBS="-lGL"
else
  SDL_CFLAGS="-I$HERE/compat"
  SDL_LIBS="-l:libSDL2-2.0.so.0"
  GL_LIBS="-l:libGL.so.1"
fi

printf '[build] compiling %s\n' "$SRC"
gcc $CFLAGS $SDL_CFLAGS "$SRC" -o "$RAW" $LDFLAGS $SDL_LIBS $GL_LIBS -lm
strip -s "$RAW"
RAW_SZ=$(stat -c%s "$RAW")
printf '[build] strip:     %s bytes\n' "$RAW_SZ"

cp "$RAW" "$SST"
if command -v sstrip >/dev/null 2>&1; then
  sstrip "$SST"
else
  python3 "$HERE/tiny_tools/sstrip64.py" "$SST"
fi
SST_SZ=$(stat -c%s "$SST")
printf '[build] sstrip:    %s bytes\n' "$SST_SZ"

xz -9e --check=crc32 --lzma2=preset=9e,pb=0,lc=4 -c "$SST" > "$SST.xz"
XZ_SZ=$(stat -c%s "$SST.xz")
printf '[build] xz stream: %s bytes\n' "$XZ_SZ"

cat > "$TIN" <<'STUB'
#!/bin/sh
t=${TMPDIR:-/tmp}/n.$$
tail -n +5 "$0"|xz -dc >"$t"&&chmod +x "$t"&&"$t" "$@"
r=$?;rm -f "$t";exit $r
STUB
cat "$SST.xz" >> "$TIN"
chmod +x "$TIN"
TIN_SZ=$(stat -c%s "$TIN")
printf '[build] runner:    %s bytes (%s KB)\n' "$TIN_SZ" "$((TIN_SZ / 1024))"
[ "$TIN_SZ" -le 98304 ] && echo '[build] UNDER 96 KB' || echo '[build] OVER 96 KB'
[ "$TIN_SZ" -le 20000 ] && echo '[build] UNDER 20,000 bytes' || echo '[build] over 20,000 bytes'
[ "$TIN_SZ" -le 20480 ] && echo '[build] UNDER 20 KiB' || echo '[build] over 20 KiB'
