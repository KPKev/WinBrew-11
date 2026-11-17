#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source=${VAPOR_BACKEND_SOURCE:-vendor/playden-source}
miniz=${VAPOR_BACKEND_MINIZ:-build/vapor-host-local-cmake/libplayden_miniz.a}
out=${VAPOR_BACKEND_TEST_OUTPUT:-build/vapor-backend-upgrade/tests}
mkdir -p "$out"
g++ -std=c++20 -O1 -pthread -DPLAYDEN_VERSION='"0.18.0"' \
  -I"$source/src" -I"$source/third_party/miniz" -I"$(dirname "$miniz")/generated" \
  src/steam_integration/backend_updates.cpp tests/vapor_backend_updates_test.cpp \
  "$source"/src/update/{version,file_ops,zip_extract,release,asset_download,app_swap,update_service}.cpp \
  "$source/src/steam/backend/json.cpp" \
  "$miniz" -o "$out/backend-updates"
"$out/backend-updates"
g++ -std=c++20 -pthread -I"$source/src" \
  src/steam_integration/preparation.cpp "$source/src/library/store.cpp" \
  tests/steam_preparation_test.cpp -o "$out/preparation"
"$out/preparation"
