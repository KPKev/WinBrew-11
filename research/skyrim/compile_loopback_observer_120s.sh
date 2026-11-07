#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
mkdir -p build/skyrim-loopback-observer
"$sdk/bin/prospero-clang" -std=c11 -DOBS_SOCKADDR_HAS_LEN -DOBS_SECONDS=120 -O2 -Wall -Wextra -Werror research/skyrim/loopback_observer.c -o build/skyrim-loopback-observer/observer-120s.elf
sha256sum research/skyrim/loopback_observer.c build/skyrim-loopback-observer/observer-120s.elf build/skyrim-loopback-observer/observer.elf
