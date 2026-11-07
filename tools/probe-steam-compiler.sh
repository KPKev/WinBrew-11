#!/usr/bin/env bash
# Compile the unmodified Playden workers and Wine 11 coordinator for the PS5 SDK.
# This is a compile-only compatibility check, not a linked executable or hardware test.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$root/vendor/playden-source/deps/ps5-payload-sdk}"
if [[ -z "${LLVM_CONFIG:-}" ]]; then
  llvm_root="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
  export LD_LIBRARY_PATH="$llvm_root/lib/x86_64-linux-gnu:$llvm_root/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export LLVM_CONFIG="$llvm_root/lib/llvm-18/bin/llvm-config"
fi
mkdir -p build/steam-probe
for source in vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/installs/install_service.cpp vendor/playden-source/src/runtime/launch_service.cpp src/steam_integration/controller.cpp src/steam_integration/downloads.cpp src/steam_integration/ready_launch.cpp src/steam_integration/playden_launch_host.cpp src/steam_integration/ui_adapter.cpp src/steam_integration/runtime.cpp; do
  output="build/steam-probe/$(basename "${source%.cpp}")-$(printf '%s' "$source" | sha256sum | cut -c1-8).o"
  bash vendor/playden-source/scripts/with-env.sh "$PS5_PAYLOAD_SDK/bin/prospero-clang++" \
    -std=c++20 -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen -c "$source" -o "$output"
done
echo 'PASS: Playden Steam, install and launch workers and Wine 11 controller compile with the PS5 SDK C++ toolchain'
