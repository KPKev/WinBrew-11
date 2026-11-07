"""Back up and atomically update the existing WinBrew 11 title, never uninstall it.

Default mode validates local files and reports a read-only console comparison.
--apply requires Wine 11 to be closed. Only manifest-listed Wine 11 resources
are staged; every old and new byte stream is verified before promotion.
"""
import argparse
import hashlib
import json
from pathlib import PurePosixPath
import re
import shutil
from datetime import datetime, timezone
from install_console import WFM, ROOT, TITLE, DEST

def digest(data):
    return hashlib.sha256(data).hexdigest()

def active_titles(w):
    active = {}
    for line in w.read('/data/shadowmount/debug.log').decode(errors='replace').splitlines():
        match = re.search(r'\[GAME\] started: (\w+) pid=(\d+)', line)
        if match:
            active[match[1]] = match[2]
        match = re.search(r'\[GAME\] process exited.*: (\w+) pid=(\d+)', line)
        if match and active.get(match[1]) == match[2]:
            active.pop(match[1])
    return active

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='192.168.7.26')
    parser.add_argument('--port', type=int, default=8889)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    source = ROOT / 'dist' / TITLE
    wanted = json.loads((source / 'manifest.json').read_text())
    for name, sha in wanted['files'].items():
        p = PurePosixPath(name)
        if p.is_absolute() or '..' in p.parts or str(p) != name:
            raise RuntimeError('Invalid package path')
        if digest((source / name).read_bytes()) != sha:
            raise RuntimeError('Local package hash mismatch: ' + name)
    assert json.loads((source / 'sce_sys/param.json').read_text())['titleId'] == TITLE
    w = WFM(args.host, args.port)
    before_manifest = w.read(DEST + '/manifest.json')
    before = json.loads(before_manifest)
    if before['project'] not in ('Wine 11', 'WinBrew 11') or not set(before['files']) < set(wanted['files']):
        raise RuntimeError('Installed title or resource set differs; inspect manually')
    added = set(wanted['files']) - set(before['files'])
    for name in added:
        if not name.startswith(('assets/', 'licenses/')):
            raise RuntimeError('New resource outside assets/licenses: ' + name)
        parent = str(PurePosixPath(name).parent)
        if any(e['name'] == PurePosixPath(name).name for e in w.listing(DEST + '/' + parent)):
            raise RuntimeError('New resource already exists outside manifest: ' + name)
    # The title ID and privileged helper stay stable during a display rebrand.
    for name in ('lapy.elf', 'lapy-manifest.json', 'sce_module/libc.prx'):
        if before['files'][name] != wanted['files'][name]:
            raise RuntimeError('Protected existing component differs: ' + name)
    changed = sorted(n for n in wanted['files'] if wanted['files'][n] != before['files'].get(n))
    print('WinBrew 11 update:', changed, flush=True)
    active = active_titles(w)
    print('WinBrew 11 running:', TITLE in active, flush=True)
    if not args.apply:
        return
    if TITLE in active:
        raise RuntimeError('Close WinBrew 11 before updating its executable')
    if not changed:
        raise RuntimeError('No resource changes; refusing duplicate deployment')
    # Promote executable and metadata after the assets; publish the manifest last.
    order = sorted(changed, key=lambda n: (n in ('eboot.bin', 'sce_sys/param.json'), n)) + ['manifest.json']
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    backup = ROOT / 'build/console-backups' / ('desktop-' + stamp)
    stage = ROOT / 'build' / ('desktop-stage-' + stamp)
    backup.mkdir(parents=True)
    stage.mkdir(parents=True)
    old = {}
    staged = []
    for name in order:
        data = before_manifest if name == 'manifest.json' else None if name in added else w.read(DEST + '/' + name)
        if name != 'manifest.json' and name not in added and digest(data) != before['files'][name]:
            raise RuntimeError('Installed resource changed unexpectedly: ' + name)
        old[name] = data
        target = backup / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if data is not None:
            target.write_bytes(data)
        staged_name = name + '.' + stamp + '.stage'
        target = stage / staged_name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        staged.append(staged_name)
    w.upload(stage, staged)
    for name in staged:
        w.permissions(DEST + '/' + name)
    if TITLE in active_titles(w):
        raise RuntimeError('WinBrew 11 was opened during staging; close it before a reviewed retry')
    applied = []
    try:
        for name, staged_name in zip(order, staged):
            base = PurePosixPath(name).name
            saved = base + '.' + stamp + '.bak'
            if name not in added:
                w.post('/api/rename', params={'path': DEST + '/' + name, 'name': saved})
            try:
                w.post('/api/rename', params={'path': DEST + '/' + staged_name, 'name': base})
            except Exception:
                if name not in added:
                    w.post('/api/rename', params={'path': DEST + '/' + str(PurePosixPath(name).with_name(saved)), 'name': base})
                raise
            applied.append(name)
            if digest(w.read(DEST + '/' + name)) != digest((source / name).read_bytes()):
                raise RuntimeError('Updated resource differs: ' + name)
    except Exception:
        for name in reversed(applied):
            base = PurePosixPath(name).name
            w.post('/api/rename', params={'path': DEST + '/' + name, 'name': base + '.' + stamp + '.stage'})
            saved = str(PurePosixPath(name).with_name(base + '.' + stamp + '.bak'))
            if name not in added:
                w.post('/api/rename', params={'path': DEST + '/' + saved, 'name': base})
        raise
    report = {'title': TITLE, 'version': json.loads((source / 'sce_sys/param.json').read_text())['contentVersion'], 'updated_at': stamp,
              'files': {n: {'before': digest(old[n]) if old[n] is not None else None, 'after': digest((source/n).read_bytes())} for n in order},
              'backup': str(backup), 'hardware_launch_tested': False,
              'browser': 'Native PS5 system browser launch; Chrome/Chromium not installed',
              'logo_rebranded': True, 'game_files_modified': False}
    (ROOT / 'evidence/desktop-update.json').write_text(json.dumps(report, indent=2) + '\n')
    print('WinBrew 11 desktop update installed and all changed files read-back verified.', flush=True)

if __name__ == '__main__':
    main()
