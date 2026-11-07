#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source_root="$PWD/vendor/playden-source/src"
output=$(mktemp)
trap 'rm -f "$output"' EXIT
"${CXX:-c++}" -std=c++20 -O2 -ffunction-sections -fdata-sections -I"$source_root" \
  tests/vapor_recipe_overlay_test.cpp \
  "$source_root/runtime/game_settings.cpp" "$source_root/runtime/prospero_profile.cpp" \
  "$source_root/steam/backend/json.cpp" \
  "$source_root/steam/backend/game_path.cpp" "$source_root/library/store.cpp" \
  -Wl,--gc-sections -o "$output"
"$output" "$PWD/assets/vapor-recipes"
