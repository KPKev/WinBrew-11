#!/usr/bin/env bash
# Refresh the changed UI snapshot after a verified build, without touching the
# already-built owner objects. Compile independent UI sources in bounded batches.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/hash-steam-native-snapshot.py verify
python3 tools/verify_steam_depot_snapshot.py
rm -f build/steam-native-experiment/ui-ready
cp src/explorer.cpp src/icons.cpp src/shell.cpp src/shell.hpp src/steam_ui.hpp build/steam-native-experiment/src/
cp src/steam_integration/ui_adapter.cpp build/steam-native-experiment/src/steam_integration/
python3 tools/hash-steam-native-snapshot.py record
export PS5_PAYLOAD_SDK="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
printf '%s\0' build/steam-native-experiment/src/*.cpp | xargs -0 -P 4 -n 1 bash -c '
  source=$1
  "$PS5_PAYLOAD_SDK/bin/prospero-clang++" -std=c++20 -O2 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
    -DPLAYDEN_PLATFORM_PS5=1 -Ibuild/steam-native-experiment/src -Ivendor/playden-source/src \
    -c "$source" -o "build/steam-native-experiment/fresh-ui/$(basename "${source%.cpp}").o"
' _
python3 tools/hash-steam-native-snapshot.py verify
touch build/steam-native-experiment/ui-ready
echo 'PASS: refreshed Vapor native interface'
