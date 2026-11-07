#!/usr/bin/env bash
set -euo pipefail
sdk="$PWD/vendor/playden-source/deps/ps5-payload-sdk";owner="$PWD/src/owner_admission";llvm="$HOME/.cache/wine11-llvm-18.1.8/root/usr"
export PS5_PAYLOAD_SDK="$sdk" LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config" LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib"
mkdir -p build/skyrim-publisher-probe
"$sdk/bin/prospero-clang" -std=c11 -D_DEFAULT_SOURCE -D_GNU_SOURCE -Wall -Wextra -Werror -Iresearch/skyrim -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h" research/skyrim/stage_bound_publisher.c research/skyrim/candidate_publish.c research/skyrim/candidate_verify.c research/skyrim/exclusive_publish.c "$owner/src/pw_publication_sha256.c" "$owner/third_party/mbedtls-publication/library/sha256.c" -o build/skyrim-publisher-probe/stage-12770a-publisher.elf
sha256sum build/skyrim-publisher-probe/stage-12770a-publisher.elf
