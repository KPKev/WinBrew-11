#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'tools')
from prepare_steam_registration_reconcile import transform
root = Path.cwd()
out = root / 'build/vapor-registration-host'
out.mkdir(parents=True, exist_ok=True)
(out / 'runtime_host.cpp').write_text(transform((root / 'vendor/playden-source/src/homebrew/runtime_host.cpp').read_text()))
# Reuse the pinned runtime's targets, with the system HTTP library for this
# host-only test. No PS5 binary or runtime-kit pin is changed.
wine = (root / 'build/steam-host-dedicated-source/cmake/WineHostProbe.cmake').read_text()
wine = wine[:wine.index('target_sources(playden_homebrew_runtime_host')]
wine = wine.replace('${CMAKE_SOURCE_DIR}/src/homebrew/runtime_host.cpp', str(out / 'runtime_host.cpp'))
(out / 'WineHostProbe.cmake').write_text(wine)
cmake = '''cmake_minimum_required(VERSION 3.24)
project(VaporRegistrationTest LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_SOURCE_DIR "@ROOT@/vendor/playden-source")
set(BUILD_TESTING OFF)
set(PLAYDEN_PLATFORM_PS5 OFF)
set(PLAYDEN_STB_IMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/stb")
set(PLAYDEN_PROSPERO_SOURCE_DIR "@ROOT@/build/steam-host-deps/prospero-win")
file(READ "${CMAKE_SOURCE_DIR}/dependencies.lock.json" playden_dependency_lock)
include("${CMAKE_SOURCE_DIR}/cmake/PlaydenCore.cmake")
add_library(playden_steam_protocol STATIC "${CMAKE_SOURCE_DIR}/src/steam/backend/json.cpp"
  "${CMAKE_SOURCE_DIR}/src/steam/backend/proto_wire.cpp")
target_include_directories(playden_steam_protocol PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(playden_steam_protocol PUBLIC cxx_std_20)
find_library(system_curl curl REQUIRED)
add_library(playden_curl_8_22 INTERFACE)
target_link_libraries(playden_curl_8_22 INTERFACE ${system_curl} mbedcrypto)
include("${CMAKE_SOURCE_DIR}/cmake/ProsperoRuntime.cmake")
target_include_directories(playden_runtime_artifact_hash PRIVATE
  "${PLAYDEN_PROSPERO_SOURCE_DIR}/third_party/mbedtls-publication/include"
  "${PLAYDEN_PROSPERO_SOURCE_DIR}/third_party/mbedtls-publication")
target_compile_definitions(playden_runtime_artifact_hash PRIVATE MBEDTLS_CONFIG_FILE="pw_publication_config.h")
target_link_libraries(playden_runtime_artifact_hash PUBLIC playden_runtime_publication_verifier)
set_source_files_properties("${CMAKE_SOURCE_DIR}/src/runtime/publication_contract.cpp"
  PROPERTIES COMPILE_OPTIONS "-include;cstdint")
include("${CMAKE_SOURCE_DIR}/cmake/SelfUpdate.cmake")
include("@OUT@/WineHostProbe.cmake")
add_executable(vapor_registration_restart_test "@ROOT@/tests/vapor_registration_restart_test.cpp")
target_include_directories(vapor_registration_restart_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(vapor_registration_restart_test PRIVATE playden_homebrew_runtime_host playden_runtime_artifact_hash)
'''
(out / 'CMakeLists.txt').write_text(cmake.replace('@ROOT@', str(root)).replace('@OUT@', str(out)))
PY
cmake -S build/vapor-registration-host -B build/vapor-registration-host/cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/vapor-registration-host/cmake --target vapor_registration_restart_test -j 6
build/vapor-registration-host/cmake/vapor_registration_restart_test "$PWD/vendor/playden-source/runtime"
