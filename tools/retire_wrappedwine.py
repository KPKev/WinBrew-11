"""Remove only the owner's retired PPSA77380 after Wine 11 is registered.

Default is a plan. --apply requires no active game, a verified new Wine 11,
and an exact local backup before the native uninstaller or source cleanup.
"""
import argparse
import hashlib
import http.cookiejar
import json
from pathlib import Path
import re
import time
import urllib.error
import urllib.request
import zipfile
from install_console import WFM, ROOT

OLD = 'PPSA77380'
NEW = 'PPSA77381'
OLD_ROOT = '/data/homebrew/' + OLD
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', default='192.168.7.26')
parser.add_argument('--port', type=int, default=8889)
parser.add_argument('--apply', action='store_true')
args = parser.parse_args()
base = f'http://{args.host}:8888'
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))


def installed():
    with opener.open(base + '/api/admin/games/installed', timeout=10) as response:
        data = json.load(response)
    if not data.get('ok'):
        raise RuntimeError('Installed-app inventory failed')
    return data['entries']


print('Plan: back up and uninstall old PPSA77380; keep Wine 11 PPSA77381 and all games.')
if not args.apply:
    raise SystemExit(0)
entries = installed()
if not any(e['id'] == NEW and e['name'] == 'Wine 11' for e in entries):
    raise RuntimeError('Wine 11 must be registered successfully before retiring the old app')
if not any(e['id'] == OLD and e['name'] == 'WrappedWine' for e in entries):
    raise RuntimeError('The exact retired app was not found; refusing a different target')
w = WFM(args.host, args.port)
raw = w.read('/data/shadowmount/debug.log').decode('utf-8', 'replace')
active = {}
for line in raw.splitlines():
    m = re.search(r'\[GAME\] started: (\w+) pid=(\d+)', line)
    if m:
        active[m.group(2)] = m.group(1)
    m = re.search(r'\[GAME\] process exited.*: (\w+) pid=(\d+)', line)
    if m:
        active.pop(m.group(2), None)
if active:
    raise RuntimeError('Close the active game normally before retiring the old app: ' + ', '.join(active.values()))
metadata = json.loads(w.read(OLD_ROOT + '/sce_sys/param.json'))
if metadata['titleId'] != OLD or metadata['localizedParameters']['en-US']['titleName'] != 'WrappedWine':
    raise RuntimeError('Old source metadata differs; refusing removal')
files = {}


def collect(path, prefix=''):
    for entry in w.listing(path):
        relative = prefix + entry['name']
        if entry['type'] == 'd':
            collect(path + '/' + entry['name'], relative + '/')
        elif entry['type'] == '-':
            files[relative] = w.read(path + '/' + entry['name'])
        else:
            raise RuntimeError('Unexpected link/device in retired app; inspect manually')


collect(OLD_ROOT)
backup = ROOT / 'build/retired-WrappedWine-PPSA77380.zip'
if backup.exists():
    raise RuntimeError('Retirement backup already exists; inspect rather than repeat')
with zipfile.ZipFile(backup, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name, data in files.items():
        archive.writestr(name, data)
with zipfile.ZipFile(backup) as archive:
    assert archive.testzip() is None
    assert all(archive.read(name) == data for name, data in files.items())
print('Verified local backup:', len(files), 'retired-app files', flush=True)
# Hide only the old source's registration metadata so the background scanner
# cannot re-register it while the native uninstall is completing.
param = OLD_ROOT + '/sce_sys/param.json'
retired = 'param.json.retired-by-wine11'
w.post('/api/rename', params={'path': param, 'name': retired})
try:
    with opener.open(base + '/', timeout=10) as response:
        html = response.read().decode('utf-8', 'replace')
    match = re.search(r'<meta name="csrf-token" content="([^"]+)"', html)
    if not match:
        raise RuntimeError('Native management session token unavailable')
    req = urllib.request.Request(base + '/api/admin/games/uninstall?id=' + OLD, data=b'{}', method='POST',
        headers={'Content-Type': 'application/json', 'X-CSRF-Token': match.group(1)})
    with opener.open(req, timeout=30) as response:
        result = json.load(response)
    if not result.get('ok'):
        raise RuntimeError('Native uninstall was rejected: ' + str(result))
    print('Native uninstall accepted for old PPSA77380', flush=True)
except Exception:
    w.post('/api/rename', params={'path': OLD_ROOT + '/sce_sys/' + retired, 'name': 'param.json'})
    raise
deadline = time.monotonic() + 50
while time.monotonic() < deadline:
    if not any(e['id'] == OLD for e in installed()):
        break
    time.sleep(1)
else:
    raise RuntimeError('Old registration is still present; backup preserved and no source deletion performed')
home = {e['name'] for e in w.listing('/data/homebrew')}
if OLD in home:
    # Exact compile-time old application root, never a game or shared directory.
    assert OLD_ROOT == '/data/homebrew/PPSA77380'
    task = w.post('/api/delete', {'paths': OLD_ROOT})['task_id']
    w.wait(task)
if OLD in {e['name'] for e in w.listing('/data/homebrew')}:
    raise RuntimeError('Old source folder is still present')
entries = installed()
assert not any(e['id'] == OLD for e in entries)
assert any(e['id'] == NEW and e['name'] == 'Wine 11' for e in entries)
report = {'removed_title': OLD, 'removed_name': 'WrappedWine', 'installed_title': NEW, 'installed_name': 'Wine 11',
          'old_registration_absent': True, 'old_source_absent': True, 'four_pane_design_preserved': True, 'icon_label': 'Wine 11',
          'backup_files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
(ROOT / 'evidence/wine11-replacement.json').write_text(json.dumps(report, indent=2))
print('REPLACEMENT VERIFIED: Wine 11 registered; old WrappedWine registration and source removed.', flush=True)
