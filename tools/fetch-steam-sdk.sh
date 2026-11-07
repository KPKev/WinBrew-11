#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source="$root/vendor/playden-source"
target="$source/deps/ps5-payload-sdk"
expected=a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb
if [[ -e "$target" ]]; then
  [[ -f "$target/.playden-source-sha256" && "$(cat "$target/.playden-source-sha256")" == "$expected" ]] || { echo 'Existing SDK is not the pinned SDK; inspect it manually.' >&2; exit 1; }
  echo 'Pinned SDK already present'; exit 0
fi
mkdir -p "$source/deps/downloads"
archive="$source/deps/downloads/ps5-payload-sdk-v0.43.zip"
if [[ ! -f "$archive" ]]; then
  curl -fsSL --retry 3 'https://github.com/ps5-payload-dev/sdk/releases/download/v0.43/ps5-payload-sdk.zip' -o "$archive.part"
  printf '%s  %s\n' "$expected" "$archive.part" | sha256sum --check --status
  mv "$archive.part" "$archive"
fi
printf '%s  %s\n' "$expected" "$archive" | sha256sum --check --status || { echo 'SDK archive hash mismatch.' >&2; exit 1; }
work=$(mktemp -d "$source/deps/.sdk-unpack.XXXXXX")
unzip -q "$archive" -d "$work"
[[ -d "$work/ps5-payload-sdk" ]] || { echo 'Unexpected SDK archive layout.' >&2; exit 1; }
printf '%s\n' "$expected" > "$work/ps5-payload-sdk/.playden-source-sha256"
mv -T "$work/ps5-payload-sdk" "$target"
rmdir "$work"
echo 'Pinned Playden SDK v0.43 ready.'
