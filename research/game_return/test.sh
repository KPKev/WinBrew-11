#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
gcc -std=c11 -O2 -Wall -Wextra -pedantic research/game_return/return_intent.c research/game_return/return_intent_test.c -o build/return-intent-test
./build/return-intent-test
gcc -std=c11 -O2 -Wall -Wextra -pedantic research/game_return/return_native.c research/game_return/return_native_test.c -o build/return-native-test
./build/return-native-test
