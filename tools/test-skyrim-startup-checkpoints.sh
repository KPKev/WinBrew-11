#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/native"
cp build/return-wrapper/source/native/wine64_main.c "$tmp/native/"
(cd "$tmp" && patch --dry-run -s -p1 < "$OLDPWD/research/skyrim/startup-checkpoints.patch")
gcc -std=c11 -D_DEFAULT_SOURCE -DPW_DATA_MOUNT_HOST_TEST -Wall -Wextra -Werror tests/skyrim_startup_checkpoints_test.c build/return-wrapper/source/native/pw_data_mount.c -o "$tmp/test"
"$tmp/test"
echo 'startup checkpoints: fallback, failed sink, errno and fixed labels passed'
