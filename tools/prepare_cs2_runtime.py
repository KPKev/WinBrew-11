"""Acquire and verify Vapor's complete CS2 runtime (no console operations)."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'assets/cs2-runtime/manifest.json'
MANIFEST_SHA256 = 'd7cf3ed3221f8e8a7eb571522363efc21a5540699b6b20b3226cf30012732259'
ARCHIVE_SHA256 = '9ce9016476f7f62f0326f2f69814612e51d0829bb48c2136a1e6d800bc4f8897'
ARCHIVE_URL = 'https://github.com/KPKev/WinBrew-11/releases/download/cs2-runtime-v1/cs2-runtime-019.1.zip'
PREFIX = 'runtime/compatibility/steam-730-cs2-v1/'
DEFAULT = ROOT / 'build/cs2-runtime-v1'
NOTICES = ROOT / 'runtime-patches/cs2/upstream-notices'

def digest(data): return hashlib.sha256(data).hexdigest()

def entries():
    raw = MANIFEST.read_bytes()
    if digest(raw) != MANIFEST_SHA256: raise ValueError('CS2 manifest differs from its compiled pin')
    manifest = json.loads(raw)
    if manifest['steam_app_id'] != 730 or manifest['id'] != 'cs2-019.1':
        raise ValueError('Unexpected CS2 runtime identity')
    result = list(manifest['files']) + [dict(path='vapor-runtime.json', size=len(raw), sha256=MANIFEST_SHA256, mode='0644')]
    seen = set()
    for entry in result:
        path = PurePosixPath(entry['path'])
        if (path.is_absolute() or '..' in path.parts or '\\' in entry['path'] or
                str(path) != entry['path'] or entry['path'] in seen or entry['mode'] not in ('0644', '0755')):
            raise ValueError('Unsafe or duplicate runtime file')
        seen.add(entry['path'])
    return result

def notice_files():
    return {'runtime/compatibility-notices/cs2-019.1/' + p.relative_to(NOTICES).as_posix(): p
            for p in NOTICES.rglob('*') if p.is_file() and not p.is_symlink()}

def expected_hashes():
    result = {PREFIX + f['path']: f['sha256'] for f in entries()}
    result.update({name: digest(path.read_bytes()) for name, path in notice_files().items()})
    return result

def verify(directory=DEFAULT):
    directory = Path(directory)
    if not directory.is_dir() or directory.is_symlink(): raise ValueError('CS2 runtime directory is missing')
    files = entries()
    actual = set()
    for path in directory.rglob('*'):
        if path.is_symlink(): raise ValueError('CS2 runtime contains a symbolic link')
        if path.is_file(): actual.add(path.relative_to(directory).as_posix())
    if actual != {f['path'] for f in files}: raise ValueError('CS2 runtime inventory differs')
    for entry in files:
        path = directory / entry['path']
        if path.stat().st_size != entry['size'] or digest(path.read_bytes()) != entry['sha256']:
            raise ValueError('CS2 runtime file differs: ' + entry['path'])
    return files

def prepare(destination=DEFAULT, archive=None):
    destination = Path(destination)
    if destination.exists(): return verify(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    cached = Path(archive) if archive else ROOT / 'build/cs2-runtime-019.1.zip'
    if not cached.exists():
        if archive: raise ValueError('Specified runtime archive is missing')
        temporary = cached.with_suffix('.download')
        try:
            with urllib.request.urlopen(ARCHIVE_URL, timeout=60) as response, temporary.open('wb') as output:
                shutil.copyfileobj(response, output)
            if digest(temporary.read_bytes()) != ARCHIVE_SHA256: raise ValueError('Downloaded runtime digest differs')
            temporary.replace(cached)
        finally:
            temporary.unlink(missing_ok=True)
    if cached.is_symlink() or digest(cached.read_bytes()) != ARCHIVE_SHA256:
        raise ValueError('Runtime archive digest differs')
    work = Path(tempfile.mkdtemp(prefix='cs2-runtime-', dir=destination.parent))
    try:
        files = entries()
        with zipfile.ZipFile(cached) as bundle:
            if bundle.namelist() != [f['path'] for f in files]: raise ValueError('Runtime archive inventory differs')
            for entry in files:
                info = bundle.getinfo(entry['path'])
                if info.file_size != entry['size'] or info.external_attr >> 16 != 0o100000 | int(entry['mode'], 8):
                    raise ValueError('Runtime archive metadata differs')
                data = bundle.read(info)
                if digest(data) != entry['sha256']: raise ValueError('Runtime archive file differs')
                path = work / entry['path']
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                os.chmod(path, int(entry['mode'], 8))
        verify(work)
        work.rename(destination)
    finally:
        if work.exists(): shutil.rmtree(work)
    return files

def check_header():
    header = (ROOT / 'src/steam_integration/cs2_runtime_files.hpp').read_text()
    if f'kCs2RuntimeManifestSha256 = "{MANIFEST_SHA256}"' not in header:
        raise ValueError('Compiled runtime manifest differs')
    rows = [f'  {{"{f["path"]}", {f["size"]}, "{f["sha256"]}", {f["mode"]}}},' for f in entries()]
    actual = [line for line in header.splitlines() if line.startswith('  {"')]
    if actual != rows: raise ValueError('Compiled runtime file list differs')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, default=DEFAULT)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    check_header()
    files = verify(args.destination) if args.verify_only else prepare(args.destination, args.archive)
    print(f'PASS: {len(files)} pinned CS2 runtime files; no console changes')
