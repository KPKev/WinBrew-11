#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source_root="$PWD/src/owner_admission"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
python3 - "$source_root" "$work" <<'PY'
import hashlib,pathlib,sys
root,work=map(pathlib.Path,sys.argv[1:])
checks={'native/pw_title_publication_native.inc':'9a3841bf42b7e70f87f180630f9ccb48d2b4e64b98185c895b0e168b4e4f6e7c','tests/test_pw_title_publication_native.inc':'8c6ef157e9d5f9f1d4817ec428b6a270bfa0913db557fc06e4d8e28828e86293','admission_observer.c':'535f0b906b1cfca500c23b33314b8681bcfcdfc333e0debda290ce78a221ddf1'}
for name, expected in checks.items():
 if hashlib.sha256((root/name).read_bytes()).hexdigest()!=expected: raise SystemExit(f'Pinned source mismatch: {name}')
base=(root/'admission_observer.c').read_text()
a=base.index('#ifdef WINE11_ADMISSION_TEST\n#define OBSERVE_TITLE')
b=base.index('\nenum {',a)
expected=base[:a]+'/* Fixed scope: never accept a caller-supplied title or app ID. */\n#define OBSERVE_TITLE \"FAKE78005\"\n#define OBSERVE_APP 489830u\n'+base[b:]
if pathlib.Path('research/skyrim/admission_observer.c').read_text()!=expected: raise SystemExit('Scoped observer differs from pinned base beyond identity')
def convert(s): return s.replace('FAKE78001','FAKE78005').replace('220','489830')
fixture=convert((root/'tests/test_pw_title_publication_native.inc').read_text())
fixture=fixture.replace('"../native/',f'"{root}/native/').replace('"../src/',f'"{root}/src/')
(work/'fixture.inc').write_text(fixture)
(work/'test.c').write_text(convert(pathlib.Path('tests/owner_admission_observer_test.c').read_text()))
PY
flags=(-O2 -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -DWINE11_ADMISSION_TEST
  -I"$source_root" -I"$source_root/native" -I"$source_root/src"
  -I"$source_root/third_party/mbedtls-publication/include" -I"$source_root/third_party/mbedtls-publication"
  '-DMBEDTLS_CONFIG_FILE="pw_publication_config.h"'
  "-DWINE11_PUBLICATION_NATIVE_SOURCE=\"$source_root/native/pw_title_publication_native.inc\""
  "-DWINE11_UPSTREAM_ADMISSION_TEST=\"$work/fixture.inc\"" -Wno-misleading-indentation)
for name in open close write chmod fchmod mkdir unlink rename chown fchown ftruncate truncate fsync flock; do flags+=("-Wl,--wrap=$name"); done
sources=(research/skyrim/admission_observer.c "$work/test.c")
for name in pw_title_publication pw_title_launch pw_game_profile pw_app_profile pw_profile_catalog pw_wine_launch pw_publication_sha256; do sources+=("$source_root/src/$name.c"); done
sources+=("$source_root/third_party/mbedtls-publication/library/sha256.c")
gcc "${flags[@]}" "${sources[@]}" -o "$work/test"
"$work/test"
