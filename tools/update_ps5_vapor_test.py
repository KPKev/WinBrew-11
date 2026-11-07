"""Safely stage and apply the isolated Vapor test update, preserving backups.

Never touches downloaded games, account state, or the production Wine 11 app.
After an interrupted operation, inspect the status and console before retrying.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import urllib.parse
import urllib.request

from install_console import WFM
from probe_ps5_kit_extraction import post, wait

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / 'build/steam-reference-v2-isolated/PPSA77382'
NEW = ROOT / 'build/vapor-isolated/PPSA77382'
STATUS = ROOT / 'build/vapor-test-update-status.json'
DEST = '/data/homebrew/PPSA77382'
ALLOWED = {'eboot.bin', 'sce_sys/param.json', 'THIRD_PARTY_NOTICES.md', 'assets/steam-header-logo.png', 'assets/vapor-icon-v1.png'}
TAG = 'vapor'


def sha(data): return hashlib.sha256(data).hexdigest()


def packages():
    manifests = []
    for package in (OLD, NEW):
        manifest = json.loads((package / 'manifest.json').read_text())
        actual = {p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file() and p.name != 'manifest.json'}
        if actual != set(manifest['files']) or any(p.is_symlink() for p in package.rglob('*')):
            raise RuntimeError('Unexpected package inventory')
        if any(sha((package / n).read_bytes()) != h for n, h in manifest['files'].items()):
            raise RuntimeError('Package verification failed')
        if json.loads((package / 'sce_sys/param.json').read_text()).get('titleId') != 'PPSA77382':
            raise RuntimeError('Unexpected test title')
        manifests.append(manifest)
    old, new = manifests
    changed = {n for n in set(old['files']) | set(new['files']) if old['files'].get(n) != new['files'].get(n)}
    if not changed or not changed <= ALLOWED or set(old['files']) - set(new['files']):
        raise RuntimeError('Unexpected changes to test package: ' + repr(changed))
    return old, new, sorted(changed)


def path(name):
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts: raise RuntimeError('Invalid package path')
    return DEST + '/' + name


def stage():
    if STATUS.exists(): raise RuntimeError('Inspect prior update before retrying')
    old, new, changed = packages(); wfm = WFM('192.168.7.26')
    if sha(wfm.read(path('manifest.json'))) != sha((OLD / 'manifest.json').read_bytes()):
        raise RuntimeError('Installed manifest differs')
    for name, expected in old['files'].items():
        if sha(wfm.read(path(name))) != expected: raise RuntimeError('Installed file differs: ' + name)
    names = changed + ['manifest.json']
    state = {'phase': 'staging', 'changed': changed, 'staged': [], 'renames': []}
    with STATUS.open('x') as f: json.dump(state, f, indent=2)
    try:
        for name in names:
            parent = str(PurePosixPath(name).parent); parent = DEST if parent == '.' else path(parent)
            leaf = PurePosixPath(name).name; stage_name = leaf + '.' + TAG + '.stage'; backup = leaf + '.before-' + TAG + '.bak'
            entries = {e['name'] for e in wfm.listing(parent)}
            if stage_name in entries or backup in entries or (name not in old['files'] and name != 'manifest.json' and leaf in entries):
                raise RuntimeError('Existing file prevents safe staging: ' + name)
            data = (NEW / name).read_bytes()
            task = post('/api/upload/prepare', {'path': parent, 'src': stage_name, 'total': str(len(data)), 'count': '1', 'rels': stage_name, 'sizes': str(len(data)), 'overwrite': '0'})['task_id']
            post('/api/upload-file', data, headers={'Content-Type': 'application/octet-stream', 'X-WFM-Task-ID': str(task), 'X-WFM-Path': urllib.parse.quote(parent, safe=''), 'X-WFM-Rel': stage_name, 'X-WFM-Size': str(len(data)), 'X-WFM-Overwrite': '0'})
            post('/api/upload/finish', params={'task_id': task}); wait(task)
            staged = parent + '/' + stage_name
            if sha(wfm.read(staged)) != sha(data): raise RuntimeError('Staged file differs: ' + name)
            task = post('/api/chmod', {'paths': staged, 'mode': '0755' if name == 'eboot.bin' else '0644', 'recursive': '0'})['task_id']; wait(task)
            state['staged'].append(name); STATUS.write_text(json.dumps(state, indent=2) + '\n')
        state['phase'] = 'staged'; STATUS.write_text(json.dumps(state, indent=2) + '\n')
        print('Vapor update staged and verified; existing app is unchanged.')
    except Exception as ex:
        state.update(phase='inspect-before-retry', error=str(ex)); STATUS.write_text(json.dumps(state, indent=2) + '\n'); raise


def apply():
    state = json.loads(STATUS.read_text()); old, new, changed = packages(); wfm = WFM('192.168.7.26')
    if state.get('phase') != 'staged' or state.get('changed') != changed: raise RuntimeError('Update is not in a verified staged state')
    with urllib.request.urlopen('http://192.168.7.26:8084/processes_list', timeout=5) as r: processes = json.load(r).get('processes', [])
    if any(str(p.get('name', '')).lower() == 'eboot.bin' for p in processes): raise RuntimeError('Close the running app before applying the update')
    for name in changed + ['manifest.json']:
        old_hash = sha((OLD / name).read_bytes()) if (OLD / name).is_file() else None
        if old_hash and sha(wfm.read(path(name))) != old_hash: raise RuntimeError('Installed file changed: ' + name)
        if sha(wfm.read(path(name) + '.' + TAG + '.stage')) != sha((NEW / name).read_bytes()): raise RuntimeError('Staged file changed: ' + name)
        parent = str(PurePosixPath(name).parent); parent = DEST if parent == '.' else path(parent)
        leaf = PurePosixPath(name).name; entries = {e['name'] for e in wfm.listing(parent)}
        if leaf + '.before-' + TAG + '.bak' in entries or (not old_hash and leaf in entries):
            raise RuntimeError('Target or backup appeared after staging: ' + name)
    try:
        state['phase'] = 'applying'; STATUS.write_text(json.dumps(state, indent=2) + '\n')
        for name in changed + ['manifest.json']:
            p = PurePosixPath(name); leaf = p.name
            if (OLD / name).is_file():
                post('/api/rename', params={'path': path(name), 'name': leaf + '.before-' + TAG + '.bak'})
                state['renames'].append([name, 'backup']); STATUS.write_text(json.dumps(state, indent=2) + '\n')
            post('/api/rename', params={'path': path(name) + '.' + TAG + '.stage', 'name': leaf})
            state['renames'].append([name, 'applied']); STATUS.write_text(json.dumps(state, indent=2) + '\n')
            if sha(wfm.read(path(name))) != sha((NEW / name).read_bytes()): raise RuntimeError('Update verification failed: ' + name)
        state['phase'] = 'updated'; STATUS.write_text(json.dumps(state, indent=2) + '\n')
        print('Updated the isolated Vapor test app; previous files retained as backups.')
    except Exception as ex:
        state.update(phase='inspect-before-retry', error=str(ex)); STATUS.write_text(json.dumps(state, indent=2) + '\n'); raise


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('phase', choices=('stage', 'apply'))
    versions = parser.add_mutually_exclusive_group()
    versions.add_argument('--shortcuts', action='store_true')
    versions.add_argument('--storage-fix', action='store_true')
    versions.add_argument('--tuning', action='store_true')
    versions.add_argument('--icons', action='store_true')
    versions.add_argument('--layout', action='store_true')
    versions.add_argument('--covers', action='store_true')
    versions.add_argument('--art-fallback', action='store_true')
    versions.add_argument('--icon-cache', action='store_true')
    versions.add_argument('--home-drag', action='store_true')
    versions.add_argument('--navigation', action='store_true')
    versions.add_argument('--badge-touchpad', action='store_true')
    versions.add_argument('--registration', action='store_true')
    versions.add_argument('--dashboard-icons', action='store_true')
    versions.add_argument('--finish-setup', action='store_true')
    versions.add_argument('--setup-confirm', action='store_true'); args = parser.parse_args()
    if args.shortcuts:
        OLD = ROOT / 'build/vapor-isolated/PPSA77382'
        NEW = ROOT / 'build/vapor-shortcuts-isolated/PPSA77382'
        STATUS = ROOT / 'build/vapor-shortcuts-update-status.json'
        TAG = 'vapor-shortcuts'
    if args.storage_fix:
        OLD = ROOT / 'build/vapor-isolated/PPSA77382'
        NEW = ROOT / 'build/vapor-storage-fix/PPSA77382'
        STATUS = ROOT / 'build/vapor-storage-fix-update-status.json'
        TAG = 'vapor-storage-fix'
    if args.tuning:
        OLD = ROOT / 'build/vapor-storage-fix/PPSA77382'
        NEW = ROOT / 'build/vapor-tuning/PPSA77382'
        STATUS = ROOT / 'build/vapor-tuning-update-status.json'
        TAG = 'vapor-tuning'
    if args.icons:
        OLD = ROOT / 'build/vapor-tuning/PPSA77382'
        NEW = ROOT / 'build/vapor-game-icons/PPSA77382'
        STATUS = ROOT / 'build/vapor-icons-update-status.json'
        TAG = 'vapor-icons'
    if args.layout:
        OLD = ROOT / 'build/vapor-game-icons/PPSA77382'
        NEW = ROOT / 'build/vapor-download-layout/PPSA77382'
        STATUS = ROOT / 'build/vapor-layout-update-status.json'
        TAG = 'vapor-layout'
    if args.covers:
        OLD = ROOT / 'build/vapor-game-icons/PPSA77382'
        NEW = ROOT / 'build/vapor-covers/PPSA77382'
        STATUS = ROOT / 'build/vapor-covers-update-status.json'
        TAG = 'vapor-covers'
    if args.art_fallback:
        OLD = ROOT / 'build/vapor-covers/PPSA77382'
        NEW = ROOT / 'build/vapor-art-fallback/PPSA77382'
        STATUS = ROOT / 'build/vapor-art-fallback-update-status.json'
        TAG = 'vapor-art-fallback'
    if args.icon_cache:
        OLD = ROOT / 'build/vapor-art-fallback/PPSA77382'
        NEW = ROOT / 'build/vapor-icon-cache/PPSA77382'
        STATUS = ROOT / 'build/vapor-icon-cache-update-status.json'
        TAG = 'vapor-icon-cache'
    if args.home_drag:
        OLD = ROOT / 'build/vapor-art-fallback/PPSA77382'
        NEW = ROOT / 'build/vapor-home-drag/PPSA77382'
        STATUS = ROOT / 'build/vapor-home-drag-update-status.json'
        TAG = 'vapor-home-drag'
    if args.navigation:
        OLD = ROOT / 'build/vapor-art-fallback/PPSA77382'
        NEW = ROOT / 'build/vapor-navigation/PPSA77382'
        STATUS = ROOT / 'build/vapor-navigation-update-status.json'
        TAG = 'vapor-navigation'
    if args.badge_touchpad:
        OLD = ROOT / 'build/vapor-art-fallback/PPSA77382'
        NEW = ROOT / 'build/vapor-badge-touchpad/PPSA77382'
        STATUS = ROOT / 'build/vapor-badge-touchpad-update-status.json'
        TAG = 'vapor-badge-touchpad'
    if args.registration:
        OLD = ROOT / 'build/vapor-badge-touchpad/PPSA77382'
        NEW = ROOT / 'build/vapor-registration/PPSA77382'
        STATUS = ROOT / 'build/vapor-registration-update-status.json'
        TAG = 'vapor-registration'
    if args.dashboard_icons:
        OLD = ROOT / 'build/vapor-registration/PPSA77382'
        NEW = ROOT / 'build/vapor-dashboard-icons/PPSA77382'
        STATUS = ROOT / 'build/vapor-dashboard-icons-update-status.json'
        TAG = 'vapor-dashboard-icons'
    if args.finish_setup:
        OLD = ROOT / 'build/vapor-dashboard-icons/PPSA77382'
        NEW = ROOT / 'build/vapor-finish-setup/PPSA77382'
        STATUS = ROOT / 'build/vapor-finish-setup-update-status.json'
        TAG = 'vapor-finish-setup'
    if args.setup_confirm:
        OLD = ROOT / 'build/vapor-finish-setup/PPSA77382'
        NEW = ROOT / 'build/vapor-setup-confirm/PPSA77382'
        STATUS = ROOT / 'build/vapor-setup-confirm-update-status.json'
        TAG = 'vapor-setup-confirm'
    stage() if args.phase == 'stage' else apply()
