#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=build/vapor-library-refresh
mkdir -p "$out"
python3 tools/prepare_vapor_library_refresh.py vendor/playden-source/src/steam/backend/entitled_library.cpp "$out/entitled_library.cpp"
python3 tools/prepare_vapor_library_refresh_tests.py vendor/playden-source/tests/entitled_library_test.cpp "$out/upstream_entitled_library_fixtures.hpp"
python3 tools/prepare_vapor_library_refresh.py --owned vendor/playden-source/src/steam/backend/library_client.cpp "$out/library_client.cpp"
g++ -std=c++20 -O1 -pthread -Ivendor/playden-source/src src/steam_integration/controller.cpp vendor/playden-source/src/steam/service.cpp vendor/playden-source/src/library/service.cpp vendor/playden-source/src/library/store.cpp tests/steam_library_refresh_test.cpp -o "$out/controller-test"
"$out/controller-test"
g++ -std=c++20 -O1 -pthread -Ivendor/playden-source/src -Ivendor/playden-source/tests "$out/entitled_library.cpp" vendor/playden-source/src/steam/backend/app_info.cpp vendor/playden-source/src/steam/backend/vdf.cpp vendor/playden-source/src/steam/backend/family_groups.cpp vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/library/store.cpp tests/steam_library_network_boundary.cpp tests/vapor_entitled_library_test.cpp -o "$out/entitled-test"
"$out/entitled-test"
g++ -std=c++20 -O1 -pthread -Ivendor/playden-source/src "$out/library_client.cpp" vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/library/store.cpp tests/steam_library_network_boundary.cpp tests/vapor_owned_library_test.cpp -o "$out/owned-test"
"$out/owned-test"
