#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++20 -O2 -Wall -Wextra -pedantic src/game_inventory.cpp tests/game_inventory_test.cpp -o build/game-inventory-test
./build/game-inventory-test
g++ -std=c++20 -O2 -Wall -Wextra -pedantic -DWINE11_NATIVE_INVENTORY_TEST src/game_inventory.cpp tests/game_inventory_native_test.cpp -o build/game-inventory-native-test
./build/game-inventory-native-test
