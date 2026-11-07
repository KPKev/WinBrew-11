#!/usr/bin/env python3
"""Check shared-launcher provenance bindings in an already validated kit.

Supplementary only: this is not the complete kit validator, a reproducible
build proof, signature verification, a console test, or a release approval.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(root, *args):
    return subprocess.run(['git', '-c', 'safe.directory=' + str(root), '-C', str(root), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout.strip()


def read_file(root, relative):
    p = PurePosixPath(relative)
    if p.is_absolute() or str(p) != relative or not p.parts or any(x in ('.', '..') for x in p.parts) or '\\' in relative:
        raise ValueError('unsafe kit path')
    path = root
    for part in p.parts:
        path = path / part
        if path.is_symlink():
            raise ValueError('symlink in kit path')
    if not path.is_file():
        raise ValueError('missing kit file')
    return path.read_bytes()


def verify(source, kit, expected_revision):
    source = Path(source).resolve(strict=True)
    kit = Path(kit).resolve(strict=True)
    if not re.fullmatch(r'[0-9a-f]{40}', expected_revision):
        raise ValueError('expected revision must be a full lowercase commit ID')
    if Path(git(source, 'rev-parse', '--show-toplevel')).resolve() != source:
        raise ValueError('source must be its own Git worktree root')
    if git(source, 'rev-parse', 'HEAD') != expected_revision:
        raise ValueError('source HEAD differs from reviewed revision')
    if git(source, 'status', '--porcelain', '--untracked-files=all'):
        raise ValueError('source worktree is not clean')
    manifest = json.loads(read_file(kit, 'manifest.json'))
    if manifest['sources']['wrapper']['revision'] != expected_revision:
        raise ValueError('kit wrapper revision differs')
    variants = [v for v in manifest['variants'] if v.get('any_title') is True]
    if len(variants) != 1:
        raise ValueError('kit must have exactly one any-title launcher')
    variant = variants[0]
    if variant['wrapper_revision'] != expected_revision:
        raise ValueError('variant revision differs')
    build_path = variant['native_build_manifest']
    eboot_path = variant['path'] + '/eboot.bin'
    build_bytes = read_file(kit, build_path)
    eboot = read_file(kit, eboot_path)
    build = json.loads(build_bytes)
    if (build.get('schema') != 'prospero-win-native-title/1' or
        build.get('source_dirty') is not False or build.get('source_revision') != expected_revision or
        build.get('title_id') != variant.get('title_id') or build.get('any_title') is not True or
        build.get('eboot_sha256') != sha(eboot)):
        raise ValueError('native build provenance or eboot differs')
    for path, data in ((build_path, build_bytes), (eboot_path, eboot)):
        entries = [x for x in manifest['files'] if x.get('path') == path]
        if len(entries) != 1 or entries[0].get('sha256') != sha(data) or entries[0].get('size') != len(data):
            raise ValueError('kit file record differs: ' + path)
    return {'source_revision': expected_revision, 'eboot_sha256': sha(eboot),
            'kit_manifest_sha256': sha(read_file(kit, 'manifest.json'))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source'); parser.add_argument('kit'); parser.add_argument('expected_revision')
    args = parser.parse_args()
    try:
        result = verify(args.source, args.kit, args.expected_revision)
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Launcher provenance check failed: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))
    print('Supplementary check only; full kit validation and target testing remain required.')

if __name__ == '__main__':
    main()
