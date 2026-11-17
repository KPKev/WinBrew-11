#!/usr/bin/env python3
"""Derive the reviewed Playden 0.28 release for Vapor's separate namespace.

This uses the independently published release manifest as its input trust
anchor. The older prospero-win packaging validator carries historical Wine/
DXVK provenance pins and rejects even the unmodified official 0.28 archive;
it is not silently weakened or presented as validation of this derivation.
The app's current kit consumer must also accept the result before deployment.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import shutil
import stat

SOURCE_MANIFEST = '38626b6020f5ba6f24d4138fd8f1b17dc3c1bca8393b3dcba8dd641510e20223'
HELPER = '76a1936fbf35b702379d479e6b856362887f0530f8a27da00c6d86bf748dc50f'
PROTOCOL = 'bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869'

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        while chunk := f.read(1024 * 1024): h.update(chunk)
    return h.hexdigest()

def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')

def derive(source, output, helper, helper_manifest):
    assert sha(source / 'manifest.json') == SOURCE_MANIFEST, 'Unexpected upstream manifest'
    manifest = json.loads((source / 'manifest.json').read_text())
    assert manifest['runtime']['artifact_revision'] == 'bd2da43-kit-0.28'
    assert manifest['kit_id'] == 'playden-kit' and len(manifest['variants']) == 1
    variant = manifest['variants'][0]
    assert variant['any_title'] and sha(source / variant['helper_protocol']) == PROTOCOL
    expected = {entry['path']: entry for entry in manifest['files']}
    actual = set()
    for path in source.rglob('*'):
        assert not path.is_symlink(), str(path)
        if path.is_dir() or path == source / 'manifest.json': continue
        rel = path.relative_to(source).as_posix()
        assert rel in expected and path.is_file(), rel
        entry = expected[rel]
        assert path.stat().st_size == entry['size'] and sha(path) == entry['sha256'], rel
        assert stat.S_IMODE(path.stat().st_mode) == int(entry['mode'], 8), rel
        actual.add(rel)
    assert actual == set(expected)
    replacement = json.loads(helper_manifest.read_text())
    rule = {'prefix': 'FAKE78', 'allow_list': '/data/wine11/title-rule/allow-list'}
    assert sha(helper) == HELPER and replacement['elf_sha256'] == HELPER
    assert replacement['protocol_sha256'] == PROTOCOL and replacement['title_rule'] == rule
    assert not output.exists() and not output.with_suffix('.building').exists()
    staged = output.with_suffix('.building')
    shutil.copytree(source, staged)
    helper_path = variant['path'] + '/lapy.elf'
    shutil.copyfile(helper, staged / helper_path)
    write_json(staged / variant['helper_manifest'], replacement)
    native = json.loads((staged / variant['native_build_manifest']).read_text())
    assert native['title_rule'] == {'prefix': 'FAKE78', 'allow_list': '/data/playden/title-rule/allow-list'}
    native['title_rule'] = rule
    write_json(staged / variant['native_build_manifest'], native)
    registry_path = manifest['template']['path'] + '/user.reg'
    registry = (staged / registry_path).read_bytes()
    old = b'Z:\\\\data\\\\playden\\\\kits\\\\playden-kit\\\\mono\\\\wine-mono-11.3.0'
    new = old.replace(b'playden\\\\kits', b'wine11\\\\kits')
    assert registry.count(old) == 1 and old != new
    (staged / registry_path).write_bytes(registry.replace(old, new))
    allowed = {helper_path, variant['helper_manifest'], variant['native_build_manifest'], registry_path}
    entries = []
    changed = set()
    for rel in sorted(expected):
        path = staged / rel
        entry = {'path': rel, 'size': path.stat().st_size, 'sha256': sha(path),
                 'mode': f'{stat.S_IMODE(path.stat().st_mode):04o}'}
        if entry != expected[rel]: changed.add(rel)
        entries.append(entry)
    assert changed == allowed, f'Unexpected kit changes: {changed ^ allowed}'
    manifest['files'] = entries
    canonical = ''.join(f"{e['path']}\t{e['size']}\t{e['sha256']}\t{e['mode']}\n" for e in entries)
    manifest['files_sha256'] = hashlib.sha256(canonical.encode()).hexdigest()
    manifest['vapor_backend'] = {'upstream_manifest_sha256': SOURCE_MANIFEST, 'upstream_version': '0.11.0',
                                 'namespace': '/data/wine11', 'console_tested': False}
    write_json(staged / 'manifest.json', manifest)
    staged.rename(output)
    print('Verified finite derivation:', len(entries), 'files;', len(changed), 'changed')
    print('Derived manifest SHA256:', sha(output / 'manifest.json'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for name in ['source', 'output', 'helper', 'helper-manifest']:
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    derive(args.source.resolve(), args.output.resolve(), args.helper.resolve(), args.helper_manifest.resolve())
