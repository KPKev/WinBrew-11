#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk"
llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
owner="$PWD/src/owner_admission"
mkdir -p build/skyrim-admission
"$sdk/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections -I"$owner" -I"$owner/native" -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h" -c research/skyrim/admission_observer.c -o build/skyrim-admission/admission_observer.o
