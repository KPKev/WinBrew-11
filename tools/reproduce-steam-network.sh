#!/usr/bin/env bash
# Rebuild pinned sources to compare with the separately preserved, console-tested libraries.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
[[ -d build/steam-network-tested-snapshot ]] || { echo 'Tested snapshot missing; refusing rebuild.' >&2; exit 1; }
expected="$(realpath "$root/vendor/playden-source/scripts/network-deps/.deps")"
[[ "$expected" == "$root/vendor/playden-source/scripts/network-deps/.deps" ]] || { echo 'Unexpected dependency cache path.' >&2; exit 1; }
export PS5_PAYLOAD_SDK="$root/vendor/playden-source/deps/ps5-payload-sdk"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export JOBS="${JOBS:-6}"
bash vendor/playden-source/scripts/network-deps/prepare-ps5-deps.sh
python3 tools/compare-steam-rebuild.py
