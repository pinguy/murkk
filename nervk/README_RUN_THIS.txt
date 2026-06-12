NERVK ARMORY GOBLIN RELEASE v3
==============================

Run prebuilt:
  ./RUN_ME.sh

Rebuild:
  ./REBUILD_ME.sh
  # or: sh ./build_goblin.sh

Important:
- .patch files are diffs. Do not run them with bash.
- To apply a patch manually: patch -p0 < file.patch
- The build script defaults to bundled tiny SDL/GL compat headers because
  distro headers made some machines produce a larger binary.
- To force system headers: USE_SYSTEM_HEADERS=1 ./build_goblin.sh
- The build script autotunes xz settings and keeps the smallest stream.

Expected result varies slightly by GCC/binutils/xz version, but should be
around 26-27 KB and only list libc.so.6 under DT_NEEDED.
