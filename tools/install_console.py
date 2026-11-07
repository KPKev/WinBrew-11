"""Install only the new Wine 11 title through an existing Web File Manager.

Default: local verification/plan only. --install performs the authorized upload.
No payload, title launch, game update, deletion or power operation is used.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import time
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
TITLE = 'PPSA77381'
DEST = '/data/homebrew/' + TITLE


class WFM:
    def __init__(self, host, port=8888):
        self.base = f'http://{host}:{port}'

    def post(self, endpoint, data=None, params=None, headers=None):
        url = self.base + endpoint
        if params:
            url += '?' + urllib.parse.urlencode(params)
        if isinstance(data, dict):
            data = urllib.parse.urlencode(data).encode()
            headers = {**(headers or {}), 'Content-Type': 'application/x-www-form-urlencoded'}
        req = urllib.request.Request(url, data=data or b'', headers=headers or {}, method='POST')
        with urllib.request.urlopen(req, timeout=120) as response:
            result = json.load(response)
        if not result.get('ok'):
            raise RuntimeError(f'{endpoint}: {result}')
        return result

    def listing(self, path):
        return self.post('/api/list', params={'path': path})['entries']

    def read(self, path):
        task = self.post('/api/download/prepare', {'paths': path})['task_id']
        with urllib.request.urlopen(self.base + '/api/download?id=' + str(task), timeout=120) as response:
            return response.read()

    def wait(self, task):
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            item = next((t for t in self.post('/api/tasks')['tasks'] if t['id'] == task), None)
            if not item:
                raise RuntimeError('Task disappeared; inspect the console before retrying')
            if item['state'] in ('done', 'completed'):
                return
            if item['state'] not in ('running', 'queued', 'pending'):
                raise RuntimeError(item)
            time.sleep(.5)
        raise RuntimeError('Task timeout; inspect the console before retrying')

    def permissions(self, path, recursive=False):
        # Scope is deliberately locked to this newly-created application.
        if path != DEST and not path.startswith(DEST + '/'):
            raise RuntimeError('Permission path outside Wine 11')
        task = self.post('/api/chmod', {'paths': path, 'mode': '0777', 'recursive': '1' if recursive else '0'})['task_id']
        self.wait(task)

    def upload(self, source, names):
        sizes = [len((source / n).read_bytes()) for n in names]
        task = self.post('/api/upload/prepare', {
            'path': DEST, 'src': names[0], 'total': str(sum(sizes)), 'count': str(len(names)),
            'rels': '\n'.join(names), 'sizes': '\n'.join(map(str, sizes)), 'overwrite': '0',
        })['task_id']
        for name, size in zip(names, sizes):
            self.post('/api/upload-file', (source / name).read_bytes(), headers={
                'Content-Type': 'application/octet-stream', 'X-WFM-Task-ID': str(task),
                'X-WFM-Path': urllib.parse.quote(DEST, safe=''),
                'X-WFM-Rel': urllib.parse.quote(name, safe=''),
                'X-WFM-Size': str(size), 'X-WFM-Overwrite': '0',
            })
            print('Uploaded', name, size, flush=True)
        self.post('/api/upload/finish', params={'task_id': task})
        for name in names:
            actual = hashlib.sha256(self.read(DEST + '/' + name)).hexdigest()
            expected = hashlib.sha256((source / name).read_bytes()).hexdigest()
            if actual != expected:
                raise RuntimeError('Read-back SHA-256 mismatch: ' + name)
            print('Verified', name, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host')
    parser.add_argument('--port', type=int, default=8888)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--resume', action='store_true', help='Resume only an unpublished staging folder whose every resource matches this package')
    args = parser.parse_args()
    source = ROOT / 'dist' / TITLE
    manifest = json.loads((source / 'manifest.json').read_text())
    for name, expected in manifest['files'].items():
        if PurePosixPath(name).is_absolute() or '..' in PurePosixPath(name).parts:
            raise RuntimeError('Invalid package path')
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise RuntimeError('Local package hash mismatch: ' + name)
    names = sorted(str(f.relative_to(source)).replace('\\', '/') for f in source.rglob('*') if f.is_file())
    metadata = 'sce_sys/param.json'
    assert json.loads((source / metadata).read_text())['titleId'] == TITLE
    print('Verified local package:', len(names), 'files. Destination:', DEST)
    print('New-title-only upload; metadata published after resource verification. No automatic launch.')
    if not args.install:
        return
    if not args.host:
        parser.error('--host is required with --install')
    wfm = WFM(args.host, args.port)
    titles = {e['name'] for e in wfm.listing('/data/homebrew')}
    existing = TITLE in titles
    if existing and not args.resume:
        raise RuntimeError('Destination already exists; refusing to overwrite it')
    if not {'FAKE77031', 'FAKE78003'} <= titles:
        raise RuntimeError('Expected Playden and main game titles were not found')
    game_path = '/data/playden-games/1089130/baseq2'
    before = {e['name']: e['size'] for e in wfm.listing(game_path)}
    required = {'pak0.pak': 183997730, 'pak1.pak': 12992754, 'pak2.pak': 45055}
    if any(before.get(n) != size for n, size in required.items()):
        raise RuntimeError('Full main-game PAK sizes differ; inspect before installation')
    if existing:
        # An interrupted first install can be resumed only before registration,
        # and only when all existing resources exactly identify this package.
        actual_names = set()
        def walk(path, prefix=''):
            for entry in wfm.listing(path):
                name = prefix + entry['name']
                if entry.get('type') == 'd':
                    walk(path + '/' + entry['name'], name + '/')
                else:
                    actual_names.add(name)
        walk(DEST)
        expected_names = set(names) - {metadata}
        if actual_names != expected_names:
            raise RuntimeError('Existing staging tree differs or is already published; refusing resume')
        for name in sorted(expected_names):
            if hashlib.sha256(wfm.read(DEST + '/' + name)).digest() != hashlib.sha256((source / name).read_bytes()).digest():
                raise RuntimeError('Staged resource differs: ' + name)
        print('Verified all unpublished staging files; resuming registration only', flush=True)
    else:
        wfm.post('/api/mkdir', params={'path': '/data/homebrew', 'name': TITLE})
    directories = sorted({str(PurePosixPath(n).parent) for n in names if str(PurePosixPath(n).parent) != '.'}, key=lambda n: (n.count('/'), n))
    if not existing:
        for directory in directories:
            p = PurePosixPath(directory)
            wfm.post('/api/mkdir', params={'path': DEST if str(p.parent) == '.' else DEST + '/' + str(p.parent), 'name': p.name})
        wfm.upload(source, [n for n in names if n != metadata])
    wfm.permissions(DEST, recursive=True)
    wfm.upload(source, [metadata])
    wfm.permissions(DEST + '/' + metadata)
    after = {e['name']: e['size'] for e in wfm.listing(game_path)}
    if before != after:
        raise RuntimeError('Game directory changed during upload; investigate concurrent activity')
    report = {'installed': True, 'launched': False, 'destination': DEST, 'host': args.host, 'port': args.port,
              'verified_files': len(names), 'game_directory_sizes_unchanged': True,
              'files': {n: hashlib.sha256((source / n).read_bytes()).hexdigest() for n in names}}
    evidence = ROOT / 'evidence' / 'wine11-install.json'
    evidence.write_text(json.dumps(report, indent=2))
    print('INSTALL VERIFIED. Awaiting first console launch and visual acceptance.', flush=True)


if __name__ == '__main__':
    main()
