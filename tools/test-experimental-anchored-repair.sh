#!/usr/bin/env bash
# Host-only fixture; never included in native build or packaging.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++20 -O2 tests/experimental_anchored_repair_test.cpp \
  -Wl,--wrap=pread -Wl,--wrap=pwrite -o build/experimental-anchored-repair-test
./build/experimental-anchored-repair-test
