#!/bin/bash
set -euo pipefail
work="$HOME/counterstrike-wine019-work"
repo="$PWD"
mkdir -p "$work/native-tool" "$work/tests"
gcc -std=c11 -O2 -Wall -Wextra -Werror -I"$repo/build/counterstrike-ntdll-build" "$repo/build/counterstrike-ntdll-build/test_fixed_hint.c" -o "$work/tests/fixed-hint"
"$work/tests/fixed-hint"
gcc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I"$repo/build/counterstrike-ntdll-build" "$repo/build/counterstrike-ntdll-build/test_fixed_hint.c" -o "$work/tests/fixed-hint-sanitize"
"$work/tests/fixed-hint-sanitize"
foundation="$repo/build/counterstrike-ntdll-foundation"
g++ -std=c++20 -O2 -Wall -Wextra -Werror "$foundation/tooling/native/native_app_builder.cpp" "$foundation/tooling/native/self_container.cpp" "$foundation/tooling/native/elf_object.cpp" "$foundation/tooling/native/sce_module_writer.cpp" -lz -o "$work/native-tool/ps5-native-tool"
printf 'MODULE TOOL READY\n'
