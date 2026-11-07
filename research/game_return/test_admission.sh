#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source_root="$PWD/build/return-wrapper/source"
python3 - "$source_root" <<'PY'
import hashlib,pathlib,subprocess,sys
root=pathlib.Path(sys.argv[1])
expected='9a3841bf42b7e70f87f180630f9ccb48d2b4e64b98185c895b0e168b4e4f6e7c'
if hashlib.sha256((root/'native/pw_title_publication_native.c').read_bytes()).hexdigest()!=expected:
    raise SystemExit('Exact installed-revision reader source hash mismatch')
test='tests/test_pw_title_publication_native.c'
original=subprocess.check_output(['git','-C',str(root),'show','ce289425bb34acbcc98f77b4e00ac98b3c16e4bc:'+test])
if (root/test).read_bytes()!=original:raise SystemExit('Upstream fixture source differs')
PY
mkdir -p build
flags=(-O2 -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -DWINE11_ADMISSION_TEST
    -I"$source_root/native" -I"$source_root/src"
    -I"$source_root/third_party/mbedtls-publication/include" -I"$source_root/third_party/mbedtls-publication"
    '-DMBEDTLS_CONFIG_FILE="pw_publication_config.h"'
    "-DWINE11_PUBLICATION_NATIVE_SOURCE=\"$source_root/native/pw_title_publication_native.c\""
    "-DWINE11_UPSTREAM_ADMISSION_TEST=\"$source_root/tests/test_pw_title_publication_native.c\""
    -Wno-misleading-indentation)
wraps=(open close write chmod fchmod mkdir unlink rename chown fchown ftruncate truncate fsync flock)
for name in "${wraps[@]}"; do flags+=("-Wl,--wrap=$name"); done
sources=(research/game_return/admission_observer.c research/game_return/admission_observer_test.c)
for name in pw_title_publication pw_title_launch pw_game_profile pw_app_profile pw_profile_catalog pw_wine_launch pw_publication_sha256; do
    sources+=("$source_root/src/$name.c")
done
sources+=("$source_root/third_party/mbedtls-publication/library/sha256.c")
gcc "${flags[@]}" "${sources[@]}" -o build/admission-observer-test
./build/admission-observer-test
