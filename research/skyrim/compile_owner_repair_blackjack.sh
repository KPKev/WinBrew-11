#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
mkdir -p build/skyrim-owner-repair
for apply in 0 1; do
  "$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror -DREPAIR_APPLY=$apply -DROOT_TITLE=\"/data/homebrew/FAKE78004\" -DROOT_DATA=\"/data/playden/games/wine11-steam-3594780\" \
    research/skyrim/owner_repair.c -o build/skyrim-owner-repair/bj-repair-apply$apply.elf
done
sha256sum research/skyrim/owner_repair.c build/skyrim-owner-repair/*.elf
