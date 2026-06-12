# .nervk armory v2

> ## Changelog — v2.1 (review pass)
> - **Bile spitter fixed.** The armory acid-gland creature's attack now commits its danger pool to the spot where you stood when it lunged, instead of the pool tracking your live position. The lit ground *is* the hitbox: it swells to its true kill radius (1.7u) over a 0.68s wind-up, so you read it and step off it. Damage on a clean hit raised to 16 (it's dodgeable now). Previously the floor-light was cosmetic and the hit was a 7u proximity+LoS gate — telegraph and hitbox now agree.
> - **Shotgun viewmodel rebuilt.** Twin barrels are now properly separated with dark bore mouths, a bright breech band, a ventilated steel rib and receiver, and warm walnut forestock/grip/stock. Seated lower-right so it no longer swallows the frame. Twin muzzle flashes aligned to the new bore spacing.
> - **Build bug fixed.** `compat/SDL2/SDL.h` was missing `SDLK_1/2/3`, `SDL_MOUSEWHEEL`, and the `wheel` member of `SDL_Event` — the symbols the armory weapon-switching added. The no-SDL2-dev `compat` fallback build was therefore broken (the author's machine took the pkg-config branch and never hit it). Now builds clean on the fallback path.
> - **Smaller-for-free.** Added safe size flags (`-fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -fno-math-errno -fno-ident`, plus `--build-id=none -z norelro --as-needed --hash-style=gnu`) and tuned the xz stream (`pb=0,lc=4`). These recovered the bytes the two fixes above added, so the runner stays ~29 KB *with* the new content. The only remaining material size lever is `dlopen`-ing SDL2/GL to shed the relocation tables (~3.5 KB), deliberately not done here to keep the build robust.
>
> Verified headless (Xvfb + llvmpipe + glReadPixels): builds on both the pkg-config and compat paths, smoke-clean, all six creatures / three weapons / three biomes render, and the self-extractor runs.


Tiny native-Linux FPS experiment in the spirit of `.kkrieger`: one C file, no asset files, no engine, procedural everything.

This branch is now built around `nervk2.c` as the base, with the useful missing pieces from `nervk_armory(5).c` folded back in rather than replacing the newer systems.

## What it is

`.nervk armory v2` is a compact endless corridor shooter:

- procedural levels carved from a deterministic seed
- procedural wall, floor, ceiling and glow textures
- normal-mapped GL 2.1 lighting and fog
- synthesized weapon and monster audio
- generated creature silhouettes
- generated weapon models
- HUD, pickups, particles, projectiles and floor transitions
- no external assets

The funny part: the current tiny build has been measured at about **28.2 KB**, while still having enough moving parts to feel like a real little horror shooter rather than just a size stunt.

## Current feature set

### Weapons

Weapon switching is on `1`, `2`, `3`, plus mouse wheel.

- `1` pistol
  - accurate hitscan
  - uses bullet ammo
  - fast, reliable fallback weapon
- `2` shotgun
  - nine-pellet spread
  - uses shell ammo
  - heavy kick
  - improved generated model with twin barrels, receiver, pump, stock and double muzzle flash
- `3` rocket launcher
  - physical glowing rocket projectile
  - smoke trail
  - radius damage
  - self-damage if you fire like a hero in a cupboard

The weapon sounds are intentionally back near the older armory-style synth sounds after the over-beefed pass made them worse.

### Pickups

Four pickup classes:

- red health
- amber/yellow bullets
- orange/brass shells
- cyan rockets

The pickup and HUD colour language follows the newer `nervk2.c` look, with shells added into the existing palette instead of recolouring the whole thing.

### Creature roster

The roster now keeps the newer `nervk2.c` enemies and the useful armory creatures as distinct kinds, so nothing gets silently overwritten.

- `CRAWLER`
  - basic fast melee pressure
- `SKITTER`
  - small, fast panic enemy
- `BRUTE`
  - large heavy tank / exit guardian
- `BRUISER`
  - renamed armory heavy variant, kept separate from `BRUTE`
- `SPITTER`
  - newer projectile spitter
  - ranged acid/gob pressure
- `ARMORY_SPITTER`
  - renamed green armory spitter/gland creature
  - does **not** fire a projectile
  - closes to bile/light range, rears up, lights the ground/player green, then applies direct damage
  - nasty when it has you boxed into a corner

That last one is important: the green armory spitter is not just another ranged shooter. It is a close-range controller / corner-punisher. The windup gives you a readable “move now” moment, but if you are trapped it becomes a proper bastard.

### Tilesets and floors

There are now three procedural tileset families:

- `WORKS`
  - original brick walls
  - grimy floor slabs
  - bolted metal ceilings
- `HIVE`
  - chitin plates
  - membrane floors
  - ribbed organic ceilings
- `VESSEL`
  - armory pressure-vessel look
  - wet black ribbed walls
  - gridded floor plates
  - oily duct ceilings

Normal floors use one coherent tileset family.

Every fourth floor is a `MIXED` floor. On those floors the wall, floor and ceiling each choose a deterministic random tileset independently, so you can get combinations like brick walls, hive floor and vessel ceiling without adding asset bulk.

### Endless descent

Touching the green exit:

- derives the next deterministic seed
- increments the floor counter
- carries resources forward with small bonuses
- changes the floor identity / tileset cycle
- keeps the pressure climbing through spawn weighting

## Controls

```text
WASD          move
Mouse         look
LMB           fire
1             pistol
2             shotgun
3             rocket launcher
Mouse wheel   cycle weapon
ESC           quit
```

## Build

Build the normal stripped ELF:

```sh
gcc -Os -Wall \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -ffunction-sections -fdata-sections \
  nervk2_armory_v2.c -o nervk2_armory_v2 \
  -Wl,--gc-sections -lSDL2 -lGL -lm
strip -s nervk2_armory_v2
```

Run it:

```sh
./nervk2_armory_v2
```

Override the deterministic seed:

```sh
./nervk2_armory_v2 --seed 12345
```

## Tiny build notes

The project still follows the original 96 KB discipline: system libraries such as SDL2, OpenGL and libc are not counted, just like `.kkrieger` leaned on the platform graphics stack.

The current tiny build has been measured at roughly:

```text
runner: 28.2 KB
```

That is after adding:

- three weapons
- four pickup types
- six creature kinds
- three procedural tileset families
- mixed-material floors
- projectiles
- particles
- synthesized audio
- HUD and generated weapon models

So the headroom is still absurdly good. There is room for this to become more of a real game without betraying the original “tiny procedural bastard” idea.

## Verification

The latest merge was syntax/type checked with strict C flags against local SDL/OpenGL stubs:

```sh
gcc -std=c99 -Wall -Wextra -Werror -fsyntax-only nervk2_armory_v2.c
```

A full native SDL2/OpenGL link needs SDL2 development headers and GL libraries installed on the target Linux system.

## Design notes

The project started as a size experiment: how small can a native Linux FPS be if everything is generated in code?

The answer is that the constraint accidentally creates a proper design language. Because there are no asset files to lean on, every feature has to be a compact system:

- textures are algorithms
- levels are seeded rules
- enemies are behaviour archetypes
- audio is small synth events
- weapon feel is timing, kick, light and hit logic
- atmosphere is material, fog and dynamic light

The result is not just “small Doom clone”. It is a tiny procedural horror shoebox where each extra kilobyte has to earn its keep.

Still no asset files. The weapons, pickups, creature silhouettes, bile-light attack, projectile trail, explosion, audio voices, floor materials and HUD are all generated in code.
