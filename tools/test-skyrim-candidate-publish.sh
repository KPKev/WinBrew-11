#!/usr/bin/env bash
set -euo pipefail
owner="$PWD/src/owner_admission"
flags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined -Iresearch/skyrim -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h")
gcc "${flags[@]}" research/skyrim/candidate_publish.c research/skyrim/candidate_verify.c research/skyrim/exclusive_publish.c "$owner/src/pw_publication_sha256.c" "$owner/third_party/mbedtls-publication/library/sha256.c" tests/skyrim_candidate_publish_test.c -o /tmp/skyrim-candidate-publish-test
stage="$(mktemp -d)"; dest="$(mktemp -d)"
cp -R build/skyrim-admission-isolated-6/boilerplate/dist/PPSA77383/. "$stage/"
/tmp/skyrim-candidate-publish-test "$dest" "$stage"

gcc "${flags[@]}" research/skyrim/candidate_publish.c research/skyrim/candidate_verify.c research/skyrim/exclusive_publish.c "$owner/src/pw_publication_sha256.c" "$owner/third_party/mbedtls-publication/library/sha256.c" tests/skyrim_candidate_publish_interference_test.c -Wl,--wrap=write -o /tmp/skyrim-candidate-publish-interference-test
dest2="$(mktemp -d)"
/tmp/skyrim-candidate-publish-interference-test "$dest2" "$stage"
printf 'PASS: changed staged bytes refused before registration metadata.\n'
