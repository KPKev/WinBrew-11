#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
base="$PWD/build/return-wrapper/source"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/native" "$tmp/tests"
for n in pw_data_mount.h pw_data_mount.c pw_lapy_elevation.h pw_lapy_elevation.c wine64_main.c; do cp "$base/native/$n" "$tmp/native/$n"; done
for n in test_pw_data_mount.c test_pw_data_mount_native.c; do cp "$base/tests/$n" "$tmp/tests/$n"; done
(cd "$tmp" && patch -s -p1 < "$OLDPWD/research/skyrim/helper-stage-diagnostic.patch")
gcc -std=c11 -D_DEFAULT_SOURCE -DPW_DATA_MOUNT_HOST_TEST -DPW_DATA_MOUNT_DIAGNOSTIC_TEST -Wall -Wextra -Werror "$tmp/tests/test_pw_data_mount.c" "$tmp/native/pw_data_mount.c" -o "$tmp/test"
"$tmp/test"
gcc -std=c11 -D_DEFAULT_SOURCE -DPW_DATA_MOUNT_HOST_TEST "-DPW_DATA_MOUNT_PATH=\"$tmp/data\"" -Wall -Wextra -Werror "$tmp/tests/test_pw_data_mount_native.c" "$tmp/native/pw_data_mount.c" -o "$tmp/native-test"
"$tmp/native-test"
