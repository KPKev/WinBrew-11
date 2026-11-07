#!/usr/bin/env bash
set -euo pipefail
root="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LD_LIBRARY_PATH="$root/lib/x86_64-linux-gnu:$root/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$root/lib/llvm-18/bin/clang" --version | head -1
"$root/lib/llvm-18/bin/llvm-config" --bindir
