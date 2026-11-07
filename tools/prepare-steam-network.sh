#!/usr/bin/env bash
# Build and verify Playden's pinned PS5 Steam networking dependencies.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/bootstrap.py
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$root/vendor/playden-source/deps/ps5-payload-sdk}"
expected_sdk=a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb
[[ -f "$PS5_PAYLOAD_SDK/.playden-source-sha256" ]] &&
  [[ "$(cat "$PS5_PAYLOAD_SDK/.playden-source-sha256")" == "$expected_sdk" ]] || {
  echo 'Steam network build requires the Playden-pinned PS5 SDK v0.43; fetch it with Playden scripts/fetch-deps.sh or set PS5_PAYLOAD_SDK.' >&2; exit 1;
}
if [[ -z "${LLVM_CONFIG:-}" ]]; then
  llvm_root="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
  if [[ -x "$llvm_root/lib/llvm-18/bin/llvm-config" ]]; then
    export LD_LIBRARY_PATH="$llvm_root/lib/x86_64-linux-gnu:$llvm_root/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LLVM_CONFIG="$llvm_root/lib/llvm-18/bin/llvm-config"
  else
    export LLVM_CONFIG="$(command -v llvm-config-18)"
  fi
fi
llvm_bindir=$("$LLVM_CONFIG" --bindir)
"$llvm_bindir/clang" --version | head -n 1 | grep -q '18\.1\.8' || {
  echo 'Steam network build requires Playden-pinned LLVM 18.1.8; set LLVM_CONFIG to that version.' >&2; exit 1;
}
bash vendor/playden-source/scripts/network-deps/prepare-ps5-deps.sh
python3 tools/verify-steam-network.py
