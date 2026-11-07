#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
mkdir -p build
python3 tools/prepare_vapor_title_remover.py build/vapor-title-remover.c
vendor/playden-source/deps/ps5-payload-sdk/bin/prospero-clang -Wall -Werror -O2 -Wl,--build-id=none \
  -o build/vapor-title-remover.elf build/vapor-title-remover.c \
  -lSceIpmi -lSceAppInstUtil -lSceUserService -lSceSystemService
printf '%s  %s\n' 'b9f421393039a3d2f3160d2b7e792883fb9aeff80d3540a9e589068500a1faf9' build/vapor-title-remover.elf | sha256sum -c -
