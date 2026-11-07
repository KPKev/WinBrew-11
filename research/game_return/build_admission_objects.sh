#!/usr/bin/env bash
# Compile only. Never run, package or deploy a console diagnostic.
set -euo pipefail
cd "$(dirname "$0")/../.."
source_root="$PWD/build/return-wrapper/source"
sdk="$PWD/vendor/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk"
out="$PWD/build/admission-observer-native"
python3 - "$source_root" <<'PY'
import hashlib,pathlib,sys
root=pathlib.Path(sys.argv[1])
expected='9a3841bf42b7e70f87f180630f9ccb48d2b4e64b98185c895b0e168b4e4f6e7c'
if hashlib.sha256((root/'native/pw_title_publication_native.c').read_bytes()).hexdigest()!=expected:
    raise SystemExit('Exact installed-revision reader source hash mismatch')
PY
mkdir -p "$out"
flags=(-std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation
    -ffunction-sections -fdata-sections
    -I"$source_root/native" -I"$source_root/src"
    -I"$source_root/third_party/mbedtls-publication/include" -I"$source_root/third_party/mbedtls-publication"
    '-DMBEDTLS_CONFIG_FILE="pw_publication_config.h"'
    "-DWINE11_PUBLICATION_NATIVE_SOURCE=\"$source_root/native/pw_title_publication_native.c\"")
cc=(env PS5_PAYLOAD_SDK="$sdk" PS5_CLANG=/usr/bin/clang-18 "$sdk/bin/prospero-clang")
sources=("$PWD/research/game_return/admission_observer.c" "$source_root/native/pw_title_publication_ps5_lock.c")
for name in pw_title_publication pw_title_launch pw_game_profile pw_app_profile pw_profile_catalog pw_wine_launch pw_publication_sha256; do
    sources+=("$source_root/src/$name.c")
done
sources+=("$source_root/third_party/mbedtls-publication/library/sha256.c")
objects=()
for path in "${sources[@]}"; do
    object="$out/$(basename "${path%.c}").o"
    "${cc[@]}" "${flags[@]}" -c "$path" -o "$object"
    objects+=("$object")
done
llvm-ar-18 rcs "$out/libwine11-readonly-admission.a" "${objects[@]}"
printf 'Compiled isolated native observer objects/archive only; no console execution or deployment.\n'
