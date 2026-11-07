#!/usr/bin/env bash
# Link Playden's Wine 11-specific game host for PS5. Never execute this probe.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/verify-steam-tested-inputs.py
python3 vendor/playden-source/tools/prospero_source.py --source build/steam-host-deps/prospero-win
[[ -f build/steam-host-cmake/libplayden_homebrew_runtime_host.a ]] || { echo 'Game host libraries are missing.' >&2; exit 1; }
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$sdk/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections \
  -c vendor/playden-source/src/platform/ps5/file_shims.c -o build/steam-host-cmake/file-shims-probe.o
network="$root/build/steam-network-tested-snapshot/lib"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -Wl,--gc-sections -Wl,--wrap=openat -Wl,--wrap=mkdirat \
  -Ibuild/steam-host-source/src tests/steam_game_host_link_probe.cpp build/steam-host-cmake/file-shims-probe.o \
  -Wl,--start-group build/steam-host-cmake/lib*.a "$network/libcurl.a" "$network/libmbedtls.a" \
  "$network/libmbedx509.a" "$network/libmbedcrypto.a" "$network/libeverest.a" "$network/libp256m.a" \
  -Wl,--end-group -o build/steam-host-cmake/game-host-probe.elf
echo 'PASS: Playden game host and runtime libraries link for PS5 (not executed)'
