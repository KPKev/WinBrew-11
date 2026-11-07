"""Acquire and verify Vapor's complete Blender runtime (no console operations)."""
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
MANIFEST = ROOT / 'assets/blender-runtime/manifest.json'
MANIFEST_SHA256 = '2191c6d66d0964fb3916cc797d992204729e3bcf3ac0db73f57a4630ef1f2644'
ARCHIVE_SHA256 = 'e26970ea6270afcf16a5bad8b3ed05fbadbbf338cb995c106773c1e41911ee84'
ARCHIVE_URL = None  # Set by the later combined release; local --archive is supported now.
PREFIX = 'runtime/compatibility/steam-365670-blender-v1/'
DEFAULT = ROOT / 'build/blender-runtime-v1'
NOTICES = ROOT / 'runtime-patches/blender/upstream-notices'

def digest(data): return hashlib.sha256(data).hexdigest()

def entries():
    raw = MANIFEST.read_bytes()
    if digest(raw) != MANIFEST_SHA256: raise ValueError('Blender manifest differs from its compiled pin')
    manifest = json.loads(raw)
    if manifest['steam_app_id'] != 365670 or manifest['id'] != 'blender-019.1':
        raise ValueError('Unexpected Blender runtime identity')
    result = (list(manifest['files']) + [dict(path='vapor-runtime.json', size=len(raw), sha256=MANIFEST_SHA256, mode='0644')] +
              list(manifest['title_files']) + [dict(path='title/vapor-compatibility.json', size=len(raw), sha256=MANIFEST_SHA256, mode='0644')])
    seen = set()
    for entry in result:
        path = PurePosixPath(entry['path'])
        if (path.is_absolute() or '..' in path.parts or '\\' in entry['path'] or
                str(path) != entry['path'] or entry['path'] in seen or entry['mode'] not in ('0644', '0755')):
            raise ValueError('Unsafe or duplicate runtime file')
        seen.add(entry['path'])
    return result

def notice_files():
    return {'runtime/compatibility-notices/blender-019.1/' + p.relative_to(NOTICES).as_posix(): p
            for p in NOTICES.rglob('*') if p.is_file() and not p.is_symlink()}

def expected_hashes():
    result = {PREFIX + f['path']: f['sha256'] for f in entries()}
    result.update({name: digest(path.read_bytes()) for name, path in notice_files().items()})
    return result

def verify(directory=DEFAULT):
    directory = Path(directory)
    if not directory.is_dir() or directory.is_symlink(): raise ValueError('Blender runtime directory is missing')
    files = entries()
    actual = set()
    for path in directory.rglob('*'):
        if path.is_symlink(): raise ValueError('Blender runtime contains a symbolic link')
        if path.is_file(): actual.add(path.relative_to(directory).as_posix())
    if actual != {f['path'] for f in files}: raise ValueError('Blender runtime inventory differs')
    for entry in files:
        path = directory / entry['path']
        if path.stat().st_size != entry['size'] or digest(path.read_bytes()) != entry['sha256']:
            raise ValueError('Blender runtime file differs: ' + entry['path'])
    return files

def prepare(destination=DEFAULT, archive=None):
    destination = Path(destination)
    if destination.exists(): return verify(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    cached = Path(archive) if archive else ROOT / 'build/blender-runtime-019.1.zip'
    if not cached.exists():
        if archive: raise ValueError('Specified runtime archive is missing')
        if not ARCHIVE_URL: raise ValueError('Supply the matching Blender runtime ZIP using --archive or place it at ' + str(cached))
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
    work = Path(tempfile.mkdtemp(prefix='blender-runtime-', dir=destination.parent))
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
    header = (ROOT / 'src/steam_integration/blender_runtime_files.hpp').read_text()
    if f'kBlenderRuntimeManifestSha256 = "{MANIFEST_SHA256}"' not in header:
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
    print(f'PASS: {len(files)} pinned Blender runtime files; no console changes')
