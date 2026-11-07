#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bundle=${1:-build/steam-network-tested-snapshot/cacert.pem}
p=src/owner_admission/third_party/mbedtls-publication
mkdir -p build
flags=(-O2 -I"$p/include" -I"$p" '-DMBEDTLS_CONFIG_FILE="pw_publication_config.h"')
gcc -std=c11 "${flags[@]}" -c "$p/library/sha256.c" -o build/steam-trust-sha.o
gcc -std=c11 "${flags[@]}" -c src/owner_admission/src/pw_publication_sha256.c -o build/steam-trust-provider.o
g++ -std=c++20 "${flags[@]}" src/steam_integration/native_trust.cpp tests/steam_native_trust_test.cpp \
  build/steam-trust-sha.o build/steam-trust-provider.o -o build/steam-native-trust-test
build/steam-native-trust-test "$bundle"
