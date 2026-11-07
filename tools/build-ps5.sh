#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/bootstrap.py
python3 tools/stage_build.py
export CXX=g++ HOST_CXX=g++ HOST_CC=gcc
export APP_SOURCE_DIR=wine11/src APP_PARAM=wine11/sce_sys/param.json
export APP_SCE_SYS=wine11/sce_sys APP_ASSETS=wine11/assets APP_LAPY_HELPER=0 USE_CCACHE=0
export APP_ROOT_FILES='wine11/LICENSE wine11/THIRD_PARTY_NOTICES.md'
export APP_INCLUDE_PATHS='wine11/src/owner_admission/native wine11/src/owner_admission/src wine11/src/owner_admission/third_party/mbedtls-publication/include wine11/src/owner_admission/third_party/mbedtls-publication'
python3 tools/prepare_native_imports.py
bash build/native-build.sh Folder
export LLVM_CONFIG="$(command -v llvm-config-18)"
python3 vendor/playden-lapy/tools/build_owned_daemon.py \
 --sdk vendor/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk \
 --logging-client vendor/ps5log --elf-helper --title PPSA77381
mkdir -p dist
cp -a vendor/ps5-native-app-boilerplate/dist/PPSA77381 dist/
cp vendor/playden-lapy/build/owned_root_helper-PPSA77381/lapy.elf dist/PPSA77381/
cp vendor/playden-lapy/build/owned_root_helper-PPSA77381/lapy-manifest.json dist/PPSA77381/
cp -a licenses dist/PPSA77381/
cp vendor/playden-lapy/LICENSE dist/PPSA77381/licenses/Lapy-MIT.txt
cp vendor/ps5-native-app-boilerplate/THIRD_PARTY_NOTICES.md dist/PPSA77381/licenses/Boilerplate-NOTICES.md
python3 tools/package.py
python3 tools/archive_source.py
