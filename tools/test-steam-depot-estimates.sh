#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
python3 tests/test_prepare_steam_depot_estimates.py
python3 tools/prepare_steam_depot_estimates.py vendor/playden-source/src/steam/backend/cm_app_details.cpp build/steam-depot-estimates.cpp
g++ -std=c++20 -O2 -ffunction-sections -fdata-sections -Ivendor/playden-source/src build/steam-depot-estimates.cpp vendor/playden-source/src/steam/backend/app_info.cpp vendor/playden-source/src/steam/backend/vdf.cpp vendor/playden-source/src/library/store.cpp tests/steam_depot_estimates_test.cpp -Wl,--gc-sections -o build/steam-depot-estimates-test
./build/steam-depot-estimates-test
