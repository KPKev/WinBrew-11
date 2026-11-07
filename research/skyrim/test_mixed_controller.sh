#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
src="$root/build/return-wrapper/source"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pthread \
  "$root/research/skyrim/test_mixed_controller.c" \
  "$src/native/pw_wine_display.c" "$src/src/pw_game_profile.c" \
  "$src/src/pw_app_profile.c" "$src/src/pw_script_input.c" \
  -o "$tmp/test_mixed_controller"
"$tmp/test_mixed_controller"
