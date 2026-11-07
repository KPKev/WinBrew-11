#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
mkdir -p build/skyrim-descriptor-stat
"$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror research/skyrim/descriptor_stat_observer.c -o build/skyrim-descriptor-stat/observer.elf
sha256sum research/skyrim/descriptor_stat_observer.c build/skyrim-descriptor-stat/observer.elf
