#!/usr/bin/env bash
set -euo pipefail
owner="$PWD/src/owner_admission"
flags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Iresearch/skyrim -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h")
gcc "${flags[@]}" research/skyrim/candidate_verify.c "$owner/src/pw_publication_sha256.c" "$owner/third_party/mbedtls-publication/library/sha256.c" tests/skyrim_candidate_verify_test.c -o /tmp/skyrim-candidate-verify-test
stage="$(mktemp -d)"
cp -R build/skyrim-admission-isolated-6/boilerplate/dist/PPSA77383/. "$stage/"
/tmp/skyrim-candidate-verify-test "$stage" 0
mv "$stage/eboot.bin" "$stage/original-eboot"
ln -s original-eboot "$stage/eboot.bin"
/tmp/skyrim-candidate-verify-test "$stage" 1
rm "$stage/eboot.bin"
mv "$stage/original-eboot" "$stage/eboot.bin"
printf x >> "$stage/eboot.bin"
/tmp/skyrim-candidate-verify-test "$stage" 1
printf 'PASS: pinned complete candidate accepted; symlink and changed eboot refused.\n'
