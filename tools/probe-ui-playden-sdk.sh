#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" PS5_PAYLOAD_SDK="$sdk"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p build/steam-probe/ui
python3 tools/stage_build.py
for source in vendor/ps5-native-app-boilerplate/wine11/src/*.cpp; do
  "$sdk/bin/prospero-clang++" -std=c++20 -fno-exceptions -fno-rtti -I vendor/ps5-native-app-boilerplate/wine11/src -c "$source" -o "build/steam-probe/ui/$(basename "${source%.cpp}").o"
done
echo 'PASS: staged Wine 11 native sources compile with Playden SDK and libc++ headers' 
