#!/usr/bin/env bash
set -euo pipefail
owner="$PWD/src/owner_admission"
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -Iresearch/skyrim -I"$owner/src" -I"$owner/third_party/mbedtls-publication/include" -I"$owner/third_party/mbedtls-publication" -include "$owner/forced_config.h" research/skyrim/candidate_publish_payload.c research/skyrim/candidate_publish.c research/skyrim/candidate_verify.c research/skyrim/exclusive_publish.c "$owner/src/pw_publication_sha256.c" "$owner/third_party/mbedtls-publication/library/sha256.c" -o /tmp/skyrim-publisher-gate-test
set +e
output=$(/tmp/skyrim-publisher-gate-test)
status=$?
set -e
[[ $status == 1 && "$output" == 'SKYRIM_PUBLISH refused=explicit-authorization-required' ]]
echo 'PASS: publisher refuses without explicit arguments before opening paths.'
