#!/usr/bin/env bash
# Host-only checks; does not stage or package the native app.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${WINE11_CACHE_TEST_OUT:-/tmp/winbrew-cache-performance}"
mkdir -p "$out/build"
if [[ ! -e "$out/assets" ]]; then ln -s "$PWD/assets" "$out/assets"; fi
if [[ ! -e "$out/tests" ]]; then ln -s "$PWD/tests" "$out/tests"; fi
run_host() { (cd "$out" && "$out/$1"); }
g++ -std=c++20 -O2 -pthread -Ivendor/playden-source/src src/steam_integration/artwork.cpp vendor/playden-source/src/steam/store_details_service.cpp vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/library/store.cpp tests/steam_artwork_service_test.cpp -o "$out/artwork"
"$out/artwork"
common=(shell vapor_shortcuts desktop_ui preferences pointer controller_input explorer privilege explorer_fileops explorer_context notepad_document notepad_ui paint image_codec game_inventory web_history tray_ui icons text_input keyboard_ui hid_input)
objects=()
for name in "${common[@]}"; do
  g++ -std=c++20 -O2 -c "src/$name.cpp" -o "$out/$name.o"
  objects+=("$out/$name.o")
done
for name in steam_ui input controller desktop3 desktop_context panel_render text_render efficiency_render; do
  g++ -std=c++20 -O2 "${objects[@]}" "tests/${name}_test.cpp" -o "$out/$name"
  run_host "$name"
done
g++ -std=c++20 -O2 "${objects[@]}" src/ps5_hid.cpp tests/native_input_test.cpp -o "$out/native_input"
run_host native_input
for source in ui_adapter runtime; do
  g++ -std=c++20 -pthread -fsyntax-only -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen "src/steam_integration/$source.cpp"
  g++ -std=c++20 -pthread -DWINE11_EXPERIMENT_UNINSTALL=1 -fsyntax-only -Ivendor/playden-source/src -Ivendor/playden-source/third_party/qrcodegen "src/steam_integration/$source.cpp"
done
