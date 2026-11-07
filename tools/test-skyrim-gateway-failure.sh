#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source_root="$PWD/build/skyrim-launch-probe-source"
test_binary=$(mktemp)
trap 'rm -f "$test_binary"' EXIT
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wno-unused-parameter -I"$source_root/native" research/skyrim/test_gateway_resolution_failure.c "$source_root/native/pw_title_publication_ps5_lock.c" -o "$test_binary"
"$test_binary"
