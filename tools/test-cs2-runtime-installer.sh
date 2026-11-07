#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
output="${CS2_TEST_ROOT:-/tmp/winbrew-cs2-auto}"
mkdir -p "$output/source"
cp -a build/vapor-host-local-source/. "$output/source/"
cp vendor/playden-source/tests/runtime_package_installer_test.cpp "$output/source/tests/"
cp tests/cs2_runtime_installer_test.cpp "$output/source/tests/"
cp src/steam_integration/cs2_runtime_files.hpp "$output/source/src/"
python3 - "$output/source" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'tools')
import prepare_cs2_runtime_installer as cs2
target=Path(sys.argv[1])
for name, transform in [('package_installer.h', cs2.package_header), ('install_service.h', cs2.install_service_header), ('package_installer.cpp', cs2.package_source)]:
    original=(Path('vendor/playden-source/src/runtime')/name).read_text()
    (target/'src/runtime'/name).write_text(transform(original))
p=target/'CMakeLists.txt'
s=p.read_text()
s+='''\nadd_executable(cs2_runtime_installer_test "${CMAKE_SOURCE_DIR}/tests/cs2_runtime_installer_test.cpp")
target_link_libraries(cs2_runtime_installer_test PRIVATE playden_runtime_package_installer playden_runtime_publication_contract)
target_include_directories(cs2_runtime_installer_test PRIVATE "${PLAYDEN_PROSPERO_SOURCE_DIR}/src" "${PLAYDEN_PROSPERO_SOURCE_DIR}/third_party/mbedtls-publication" "${PLAYDEN_PROSPERO_SOURCE_DIR}/third_party/mbedtls-publication/include")
target_compile_definitions(cs2_runtime_installer_test PRIVATE MBEDTLS_CONFIG_FILE="pw_publication_config.h")
'''
p.write_text(s)
PY
cmake -S "$output/source" -B "$output/cmake" -G Ninja \
  -DPLAYDEN_PROSPERO_SOURCE_DIR="$root/build/steam-host-deps/prospero-win" \
  -DPLAYDEN_STEAM_PREFIX="$root/build/vapor-host-network" > "$output/configure.log" 2>&1
cmake --build "$output/cmake" --target cs2_runtime_installer_test -j 4 > "$output/build.log" 2>&1
"$output/cmake/cs2_runtime_installer_test" "$@"
