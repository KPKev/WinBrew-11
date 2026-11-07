#!/usr/bin/env bash
# Host-only parser check. Does not install or run any game.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
runtime=${1:-"$root/vendor/playden-source/runtime"}
[[ "$runtime" = /* && -d "$runtime" ]] || { echo 'Use an absolute runtime directory.' >&2; exit 1; }
g++ -std=c++20 -O2 -ffunction-sections -fdata-sections -Ivendor/playden-source/src \
 vendor/playden-source/src/runtime/game_settings.cpp vendor/playden-source/src/platform/managed_directory.cpp \
 vendor/playden-source/src/library/store.cpp vendor/playden-source/src/runtime/prospero_profile.cpp \
 vendor/playden-source/src/steam/backend/game_path.cpp tests/steam_compatibility_catalog_test.cpp \
 -Wl,--gc-sections -o build/steam-compatibility-catalog-test
./build/steam-compatibility-catalog-test "$runtime"
