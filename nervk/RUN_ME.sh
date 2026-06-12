#!/bin/sh
set -eu
cd "$(dirname "$0")"
chmod +x ./nervk_armory_goblin.xzrun
exec ./nervk_armory_goblin.xzrun "$@"
