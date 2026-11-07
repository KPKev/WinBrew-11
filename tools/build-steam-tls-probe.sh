#!/usr/bin/env bash
# Isolated PS5 ELF diagnostic, not a Wine 11 title or release artifact.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
python3 tools/verify-steam-network.py --headers-only
sdk="$root/vendor/playden-source/deps/ps5-payload-sdk"
network="$root/vendor/playden-source/scripts/network-deps/.deps/ps5"
llvm="${WINE11_LLVM_CACHE:-$HOME/.cache/wine11-llvm-18.1.8}/root/usr"
export LLVM_CONFIG="$llvm/lib/llvm-18/bin/llvm-config"
export LD_LIBRARY_PATH="$llvm/lib/x86_64-linux-gnu:$llvm/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
build="$root/build/steam-tls-probe"
mkdir -p "$build"
python3 - "$network/cacert.pem" "$root/vendor/playden-source/tests/fixtures/http/wrong-ca.pem" "$build/steam_probe_ca.h" <<'PY'
import hashlib, pathlib, sys
ca=pathlib.Path(sys.argv[1]).read_bytes()
wrong=pathlib.Path(sys.argv[2]).read_bytes()
assert hashlib.sha256(ca).hexdigest()=='a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505'
assert hashlib.sha256(wrong).hexdigest()=='30fb4ec4597d397807e59a68943ccd555cf7aa3976fb55b9fc7eba61de8b9620'
def array(name, data): return 'static const unsigned char '+name+'[] = {'+','.join(str(b) for b in data)+',0};\n'
pathlib.Path(sys.argv[3]).write_text(array('wine11_probe_ca',ca)+array('wine11_wrong_ca',wrong))
PY
"$sdk/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$network/include" -I"$build" tools/diagnostics/ps5_steam_tls_probe.c \
  -Wl,--start-group "$network/lib/libcurl.a" "$network/lib/libmbedtls.a" "$network/lib/libmbedx509.a" \
  "$network/lib/libmbedcrypto.a" "$network/lib/libeverest.a" "$network/lib/libp256m.a" -Wl,--end-group \
  -lSceNet -o "$build/probe.elf"
sha256sum "$build/probe.elf"
echo 'Diagnostic only: networking archives differ from Playden pins; do not package as a release.'
