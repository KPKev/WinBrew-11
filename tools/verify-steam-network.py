"""Verify the pinned Playden PS5 Steam TLS inputs before they are used in Wine 11."""
from pathlib import Path
import hashlib
import json
import sys

root = Path(__file__).resolve().parents[1]
source = root / 'vendor/playden-source'
lock = json.loads((source / 'dependencies.lock.json').read_text())['steam_network_probe']
prefix = source / 'scripts/network-deps/.deps/ps5'
if sys.argv[1:] not in ([], ['--headers-only']):
    sys.exit('Usage: verify-steam-network.py [--headers-only]')
headers_only = bool(sys.argv[1:])
expected = {} if headers_only else {prefix / 'lib' / name: digest for name, digest in lock['ps5_archives'].items()}
expected[prefix / 'cacert.pem'] = lock['ca_bundle']['sha256']
expected[prefix / 'include/mbedtls/mbedtls_config.h'] = lock['mbedtls']['config_sha256']
for name, digest in lock['public_headers'].items():
    expected[prefix / 'include' / name] = digest
actual_curl_headers = {p.relative_to(prefix / 'include').as_posix() for p in (prefix / 'include/curl').glob('*.h')}
if actual_curl_headers != set(lock['public_headers']):
    sys.exit('Steam TLS verification failed: curl header set differs from the pinned lock')
mismatches = [str(path.relative_to(prefix)) for path, digest in expected.items()
              if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest]
if mismatches:
    sys.exit('Steam TLS verification failed: missing or changed ' + ', '.join(mismatches))
print('PASS: ' + ('curl headers, TLS configuration and CA bundle' if headers_only else 'Playden PS5 Steam archives, curl headers, TLS configuration and CA bundle') + ' match the lock')
