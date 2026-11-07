#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/steam-staged-hash-probe
export PS5_PAYLOAD_SDK="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
"$PS5_PAYLOAD_SDK/bin/prospero-clang++" -std=c++20 -O2 -DPLAYDEN_PLATFORM_PS5=1 \
 -Ibuild/steam-host-dedicated-source/src -Ibuild/steam-network-tested-snapshot/include \
 tools/diagnostics/ps5_steam_staged_kit_hash_probe.cpp \
 build/steam-host-dedicated-source/src/runtime/artifact_hash.cpp \
 build/steam-host-dedicated-source/src/platform/managed_directory.cpp \
 build/steam-host-dedicated-source/src/steam/backend/json.cpp \
 build/steam-network-tested-snapshot/lib/libmbedcrypto.a -o build/steam-staged-hash-probe/probe.elf
sha256sum build/steam-staged-hash-probe/probe.elf
