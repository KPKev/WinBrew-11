#!/usr/bin/env bash
# Host-only syscall characterization. It does not build or deploy a remover.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++20 -O2 tests/steam_unlink_identity_race_test.cpp -o build/steam-unlink-identity-race-test
./build/steam-unlink-identity-race-test
echo 'PASS: no-follow traversal and final-name replacement were characterized on the host'
