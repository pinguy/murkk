# .nervk armory

Endless `.kkrieger`-spirit Linux FPS variant.

Built from the endless `.nervk` branch and expanded with:

- weapon switching on `1`, `2`, `3`
  - `1` pistol: accurate hitscan, bullet ammo
  - `2` shotgun: nine pellet spread, shell ammo, heavy kick
  - `3` rocket launcher: glowing physical rocket, smoke trail, radius damage, self-damage
- four pickup classes
  - red health
  - yellow bullets
  - orange shells
  - green rockets
- three creature kinds
  - cable crawler: fast melee insect
  - bruiser: tall slow hard hitter
  - spitter: squat green ranged gland
- second procedural tileset
  - odd floors use wet black ribbed walls, gridded floor plates, oily duct ceilings
  - even floors use the original brick/floor/bolted-metal set
- endless descent still active: touching the green exit derives the next deterministic seed, carries resources forward, and increments the floor counter.

## Controls

```text
WASD      move
Mouse     look
LMB       fire
1 / 2 / 3 pistol / shotgun / rocket launcher
ESC       quit
```

## Build

Normal stripped ELF:

```sh
gcc -Os -Wall \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -ffunction-sections -fdata-sections \
  nervk_armory.c -o nervk_armory \
  -Wl,--gc-sections -lSDL2 -lGL -lm
strip -s nervk_armory
```

Tiny runner path:

```sh
./build_nervk_armory_tiny.sh
```

The build script uses `strip`, then `sstrip` if installed, otherwise the included Python `tiny_tools/sstrip64.py`, then appends an xz stream to a tiny shell runner.

## Measured in this container

```text
strip:     60,128 bytes
sstrip:    57,972 bytes
xz stream: 23,524 bytes
runner:    23,636 bytes
```

So the expanded armory build is still comfortably under the original 96 KB discipline, but it no longer fits under 20 KB with the shell/xz runner. The earlier smaller build did; the rocket/projectile/enemy/tileset pass spent the headroom.

## Verification

Smoke-tested under Xvfb + llvmpipe:

```text
[nervk] SMOKE OK
```

The smoke path synthesizes textures, carves a level, renders title/game frames, and exits 0.

Still no asset files. The new weapons, pickups, creature silhouettes, projectile trail, explosion, audio voices, and second tileset are all generated in code.
