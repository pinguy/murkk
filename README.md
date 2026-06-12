# .murkk

A 96K-spirit homage to **.kkrieger** (.theprodukkt, Breakpoint 2004). Native Linux,
single C file, no engine, no assets. Everything you see and hear is synthesized
at startup or runtime: the textures, the level, the creatures, the gun, the
sounds, the font.

**Stripped binary: 51,336 bytes.** Budget: 98,304. Headroom: ~46 KB.

## Build

Arch / CachyOS:

    sudo pacman -S --needed sdl2 mesa gcc
    ./build.sh

Debian / Ubuntu:

    sudo apt install libsdl2-dev libgl1-mesa-dev gcc
    ./build.sh

Or just: `gcc -Os murkk.c -o murkk -lSDL2 -lGL -lm`

## Play

    ./murkk                # fixed seed — it's a demo, same world every run
    ./murkk --seed 1982    # different complex
    ./murkk --smoke        # headless self-test: renders, screenshots, exits 0

WASD move, mouse look, LMB fire, ESC quit. The creatures are low — aim down,
same as the original's spiders. Find the green beacon. Red cubes heal,
yellow cubes are ammo.

## What is synthesized where

- **Textures** (startup, ~260 ms): tileable value-noise fBm over power-of-two
  lattices → heightfields (brick bevels, plate insets, bolt bumps, brushed
  streaks) → Sobel-derived tangent-space normal maps + albedo ramps with
  per-brick hash jitter, grime, and rust thresholds. Four 256² maps, zero
  files.
- **Level** (startup, <1 ms): seeded xorshift carves rooms, chains them with
  L-corridors, places lights (alternating warm sodium / cold mercury — the
  kkrieger murk), items, creatures, exit. Geometry batched into interleaved
  client arrays; tangent frames derived from axis-aligned normals identically
  in C and GLSL so the bump mapping lines up by construction.
- **Lighting**: GLSL 120, eight nearest point lights per frame with quadratic
  falloff, per-pixel normal mapping on all level surfaces, distance fog,
  muzzle-flash and impact lights injected as temporaries, sqrt gamma-out.
- **Creatures**: box-assembled quadrupeds posed per frame by column-major mat3
  composition — sin-swung legs while chasing, rear-up windup before the lunge,
  squash-and-stay-dead on kill. Emissive eyes.
- **Audio**: one SDL float callback mixing a detuned-55 Hz drone with filtered
  rumble under one-shot synth voices — noise-burst gunshot with a pitch-glide
  thump, hit ticks, descending-saw death, footsteps, pickup chirps. No
  samples.
- **Font**: 37 glyphs, 5×7, hand-packed bit array, 259 bytes.

## Verification

`--smoke` runs headless under Xvfb + llvmpipe: synthesizes everything, renders
the title and a staged firefight, writes both frames as PPM, exits 0. This
build was verified that way — the screenshots in this folder are its actual
output, software-rasterized.

## Honest deltas from real 96K practice

kkrieger's 96 KB was won with tooling this homage deliberately skips, so the
51 KB here is gentleman's-rules size-coding, not compo-grade:

- No packer. kkrieger shipped through **kkrunchy** (ryg's executable
  compressor). The Linux equivalents — `sstrip`, a `vondehi`-style stub over
  xz/zstd — would put this binary somewhere under 20 KB.
- No `-nostartfiles`/custom `_start`, no shader minification, no symbol diet.
  All left on the table as headroom.
- Dynamic linking against system SDL2/libGL/libc, excluded from the budget the
  same way kkrieger leaned on d3d9.dll and the MSVC runtime.
- kkrieger's levels were hand-authored inside werkkzeug, their operator-stack
  tool; mine are generated. Different trade, same philosophy: the level is
  data born at runtime, not bytes on disk.
- The RAM trade is faithfully reproduced: kkrieger famously unpacked into
  hundreds of megabytes of 2004-era RAM from its 96 KB. Disk was the only
  budget. Same here.

CC0 / public domain. Greets to farbrausch and .theprodukkt.
