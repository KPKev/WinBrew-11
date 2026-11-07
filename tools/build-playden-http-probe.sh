#!/usr/bin/env bash
# Build the read-only Playden HTTP diagnostic; not a Wine 11 release.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/verify-steam-network.py --headers-only
python3 - <<'PY'
import hashlib,json,pathlib
r=pathlib.Path('.')
tested=json.loads((r/'evidence/steam-network-toolchain-check.json').read_text())['archives']
for name,item in tested.items():
 p=r/'vendor/playden-source/scripts/network-deps/.deps/ps5/lib'/name
 if hashlib.sha256(p.read_bytes()).hexdigest()!=item['actual']: raise SystemExit('Archive differs from console-tested build: '+name)
print('PASS: archives match the isolated console-tested build')
PY
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
network="$root/vendor/playden-source/scripts/network-deps/.deps/ps5"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build="$root/build/playden-http-probe"; mkdir -p "$build"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -DPLAYDEN_PLATFORM_PS5=1 \
 -Ivendor/playden-source/src -I"$network/include" \
 tools/diagnostics/ps5_playden_http_probe.cpp vendor/playden-source/src/steam/backend/http_client.cpp \
 vendor/playden-source/src/steam/backend/native_network.cpp \
 -Wl,--start-group "$network/lib/libcurl.a" "$network/lib/libmbedtls.a" "$network/lib/libmbedx509.a" \
 "$network/lib/libmbedcrypto.a" "$network/lib/libeverest.a" "$network/lib/libp256m.a" -Wl,--end-group \
 -lSceNet -o "$build/probe.elf"
sha256sum "$build/probe.elf"
echo 'Diagnostic only: normal Playden archive-pin gate remains unchanged.'
