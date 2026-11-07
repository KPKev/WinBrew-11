#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/dns-resolver-probe
export LLVM_CONFIG=/usr/bin/llvm-config-18
vendor/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk/bin/prospero-clang \
  -std=c11 -O2 -Wall -Wextra -Werror tools/diagnostics/ps5_dns_probe.c \
  -lSceNet -o build/dns-resolver-probe/probe.elf
sha256sum build/dns-resolver-probe/probe.elf
