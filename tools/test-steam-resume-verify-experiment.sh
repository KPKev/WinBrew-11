#!/usr/bin/env bash
# Host and SDK compile experiment only. No packaging or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
WINE11_EXPERIMENT_RESUME_VERIFY=1 python3 tools/prepare-steam-depot-tuning.py build/test-resume-verify-depot.cpp
g++ -std=c++20 -O1 -pthread -ffunction-sections -fdata-sections -Isrc -Ivendor/playden-source/src -Ibuild/steam-network-tested-snapshot/include \
  build/test-resume-verify-depot.cpp vendor/playden-source/src/steam/backend/cdn_client.cpp vendor/playden-source/src/steam/backend/game_path.cpp \
  vendor/playden-source/src/library/store.cpp vendor/playden-source/src/steam/install_volumes.cpp tests/steam_resume_verify_worker_test.cpp \
  build/steam-network-tested-snapshot/lib/libmbedcrypto.a -Wl,--gc-sections -Wl,--wrap=open -Wl,--wrap=read -o build/steam-resume-verify-worker-test
build/steam-resume-verify-worker-test
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -DPLAYDEN_PLATFORM_PS5=1 -Isrc -Ivendor/playden-source/src \
  -Ibuild/steam-network-tested-snapshot/include -c build/test-resume-verify-depot.cpp -o build/test-resume-verify-native.o
