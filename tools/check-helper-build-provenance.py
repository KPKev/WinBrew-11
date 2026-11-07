"""Read-only comparison of recorded helper build identities; no console access."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/wine11-lapy-source/build/owned_root_helper-FAKE78-rule'
SDK = ROOT / 'vendor/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk'
OLD_BUILD_ID = 'dd59bed8182966ad2c66ad5ea4179b58027b2317d852a64a65fd09587542cdb5'
NEW_PATH = '/data/wine11/title-rule/allow-list'
OLD_PATH = '/data/playden/title-rule/allow-list'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def main():
    d = json.loads((BUILD / 'manifest.json').read_text(encoding='utf-8'))
    command = json.loads((BUILD / 'build.log').read_text(encoding='utf-8').splitlines()[0])
    flags = [x for x in command if x.startswith('-std=') or x in ('-O2', '-Wall', '-Wextra', '-Werror') or x.startswith(('-DLAPY_', '-DTARGET_TITLE='))]
    for name, expected in d['inputs_sha256'].items():
        path = ROOT / 'vendor/ps5log/ps5log.h' if name == 'logging/ps5log.h' else ROOT / 'build/wine11-lapy-source' / name
        if digest(path) != expected: raise ValueError('Helper source input differs: ' + name)
    expected_sdk = d['sdk_inputs_sha256']
    actual_files = {p.relative_to(SDK).as_posix() for p in (SDK / 'target').rglob('*') if p.is_file()}
    if actual_files != set(expected_sdk): raise ValueError('SDK file inventory differs')
    for name, expected in expected_sdk.items():
        if digest(SDK / name) != expected: raise ValueError('SDK input differs: ' + name)
    def build_id(f):
        return hashlib.sha256(json.dumps({'inputs': d['inputs_sha256'], 'sdk': expected_sdk, 'flags': f}, sort_keys=True).encode()).hexdigest()
    if build_id(flags) != d['build_id']: raise ValueError('Current build identity mismatch')
    if sum(NEW_PATH in f for f in flags) != 1: raise ValueError('Unexpected title-rule flags')
    old_flags = [f.replace(NEW_PATH, OLD_PATH) for f in flags]
    if build_id(old_flags) != OLD_BUILD_ID: raise ValueError('Original build identity mismatch')
    print('PASS: recorded source and SDK hashes match; replacing only the list-path flag reproduces the recorded Quake helper build ID')
    print('This comparison does not establish helper execution or console compatibility.')

if __name__ == '__main__':
    main()
