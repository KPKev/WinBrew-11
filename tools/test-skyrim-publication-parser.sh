#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source_root="$PWD/build/return-wrapper/source"
test -f "$source_root/src/pw_title_publication.c"
test_binary=$(mktemp)
trap 'rm -f "$test_binary"' EXIT
gcc -std=c11 -Wall -Wextra -Werror -I"$source_root/src" research/skyrim/test_publication_parse.c "$source_root/src/pw_title_publication.c" -o "$test_binary"
"$test_binary"
