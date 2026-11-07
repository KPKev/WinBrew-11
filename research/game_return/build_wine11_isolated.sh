#!/usr/bin/env bash
# Build a separate review candidate only. No install/update/console command.
set -euo pipefail
cd "$(dirname "$0")/../.."
project="$PWD/build/owner-observer-integration/project"
phase=${1:-baseline}
[[ $phase == baseline || $phase == integrated ]] || exit 2
[[ -d $project/src && -d $project/vendor/ps5-native-app-boilerplate/.deps/native ]] || exit 2
cd "$project"
python3 tools/bootstrap.py
python3 tools/stage_build.py
python3 tools/prepare_native_imports.py
export CXX=g++ HOST_CXX=g++ HOST_CC=gcc
export APP_SOURCE_DIR=wine11/src APP_PARAM=wine11/sce_sys/param.json
export APP_SCE_SYS=wine11/sce_sys APP_ASSETS=wine11/assets APP_LAPY_HELPER=0 USE_CCACHE=0
export APP_ROOT_FILES='wine11/LICENSE wine11/THIRD_PARTY_NOTICES.md'
if [[ $phase == integrated ]]; then
    export APP_INCLUDE_PATHS='wine11/src/owner_admission/native wine11/src/owner_admission/src wine11/src/owner_admission/third_party/mbedtls-publication/include wine11/src/owner_admission/third_party/mbedtls-publication'
fi
bash build/native-build.sh Folder
out="$project/../$phase"
mkdir -p "$out"
cp vendor/ps5-native-app-boilerplate/dist/PPSA77381/eboot.bin "$out/eboot.bin"
cp vendor/ps5-native-app-boilerplate/build/llvm-pie.elf "$out/llvm-pie.elf"
llvm-readelf-18 --dyn-syms "$out/llvm-pie.elf" | awk '$7 == "UND" { print $8 }' | sort -u > "$out/imports.txt"
printf 'Isolated %s Wine11 build complete. No deployment.\n' "$phase"
