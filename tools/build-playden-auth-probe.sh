#!/usr/bin/env bash
# Ephemeral QR-auth test; no account, credential persistence or release build.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/verify-steam-network.py --headers-only
python3 - <<'PY'
import hashlib,json,pathlib
r=pathlib.Path('.'); evidence=json.loads((r/'evidence/steam-network-console-tls.json').read_text())
assert evidence['status']=='isolated_console_tls_check_passed_not_release_qualified'
tested=json.loads((r/'evidence/steam-network-toolchain-check.json').read_text())['archives']
for name,item in tested.items():
 p=r/'vendor/playden-source/scripts/network-deps/.deps/ps5/lib'/name
 if hashlib.sha256(p.read_bytes()).hexdigest()!=item['actual']: raise SystemExit('Archive differs from tested build: '+name)
print('PASS: archives match the isolated console-tested build')
PY
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"; network="$root/vendor/playden-source/scripts/network-deps/.deps/ps5"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build="$root/build/playden-auth-probe"; mkdir -p "$build"
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -DPLAYDEN_PLATFORM_PS5=1 \
 -Ivendor/playden-source/src -I"$network/include" \
 tools/diagnostics/ps5_playden_auth_probe.cpp vendor/playden-source/src/steam/service.cpp \
 vendor/playden-source/src/steam/backend/auth_client.cpp vendor/playden-source/src/steam/backend/auth_messages.cpp \
 vendor/playden-source/src/steam/backend/proto_wire.cpp vendor/playden-source/src/steam/backend/cm_messages.cpp \
 vendor/playden-source/src/steam/backend/json.cpp vendor/playden-source/src/steam/backend/token_claims.cpp \
 vendor/playden-source/src/steam/backend/password_crypto.cpp vendor/playden-source/src/steam/backend/http_client.cpp \
 vendor/playden-source/src/steam/backend/native_network.cpp \
 -Wl,--start-group "$network/lib/libcurl.a" "$network/lib/libmbedtls.a" "$network/lib/libmbedx509.a" \
 "$network/lib/libmbedcrypto.a" "$network/lib/libeverest.a" "$network/lib/libp256m.a" -Wl,--end-group \
 -lSceNet -o "$build/probe.elf"
sha256sum "$build/probe.elf"
echo 'Ephemeral diagnostic only; no release verification bypass.'
