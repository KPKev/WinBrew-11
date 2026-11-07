#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror "$root/research/skyrim/test_input_toggle.c" -o "$tmp/test_input_toggle"
"$tmp/test_input_toggle"
