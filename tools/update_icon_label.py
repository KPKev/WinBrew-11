"""Replace only Wine 11's source/cache icons and package manifest."""
import hashlib
import json
from pathlib import Path
import shutil
import urllib.parse
from install_console import WFM, ROOT, DEST

w = WFM('192.168.7.26', 8889)
new = (ROOT / 'sce_sys/icon0.png').read_bytes()
old_hash = '80fee2fe115e2fcefd8a8262d3175fac50f602179a2d21ddd601f0a26948343f'
backup = ROOT / 'build/icon-label-backup'
backup.mkdir(exist_ok=True)
records = []


def replace(parent, name, data, label, expected=None):
    old = w.read(parent + '/' + name)
    if old == data:
        print('Already current:', label, flush=True)
        return
    if expected and hashlib.sha256(old).hexdigest() != expected:
        raise RuntimeError('Existing icon differs from the recorded original: ' + label)
    (backup / label).write_bytes(old)
    stage = name + '.wine11-label.stage'
    previous = name + '.before-wine11-label.bak'
    entries = {e['name'] for e in w.listing(parent)}
    if stage in entries or previous in entries:
        raise RuntimeError('A prior icon update exists; inspect rather than overwrite')
    task = w.post('/api/upload/prepare', {'path': parent, 'src': stage, 'total': str(len(data)), 'count': '1',
        'rels': stage, 'sizes': str(len(data)), 'overwrite': '0'})['task_id']
    w.post('/api/upload-file', data, headers={'Content-Type': 'application/octet-stream',
        'X-WFM-Task-ID': str(task), 'X-WFM-Path': urllib.parse.quote(parent, safe=''),
        'X-WFM-Rel': urllib.parse.quote(stage, safe=''), 'X-WFM-Size': str(len(data)), 'X-WFM-Overwrite': '0'})
    w.post('/api/upload/finish', params={'task_id': task})
    if w.read(parent + '/' + stage) != data:
        raise RuntimeError('Icon staging read-back differs')
    w.post('/api/rename', params={'path': parent + '/' + name, 'name': previous})
    try:
        w.post('/api/rename', params={'path': parent + '/' + stage, 'name': name})
    except Exception:
        w.post('/api/rename', params={'path': parent + '/' + previous, 'name': name})
        raise
    if w.read(parent + '/' + name) != data:
        raise RuntimeError('Promoted icon read-back differs')
    records.append({'path': parent + '/' + name, 'sha256': hashlib.sha256(data).hexdigest()})
    print('Verified:', label, flush=True)


assert DEST == '/data/homebrew/PPSA77381'
replace(DEST + '/sce_sys', 'icon0.png', new, 'source-icon.png', old_hash)
replace('/user/appmeta/PPSA77381', 'icon0.png', new, 'cached-icon.png', old_hash)
shutil.copy2(ROOT / 'sce_sys/icon0.png', ROOT / 'dist/PPSA77381/sce_sys/icon0.png')
import subprocess
subprocess.run(['python', str(ROOT / 'tools/package.py')], check=True)
replace(DEST, 'manifest.json', (ROOT / 'dist/PPSA77381/manifest.json').read_bytes(), 'manifest.json')
(ROOT / 'evidence/wine11-icon-label.json').write_text(json.dumps({
    'label': 'Wine 11', 'four_pane_symbol_unchanged': True, 'source_and_cached_icons_verified': True,
    'sha256': hashlib.sha256(new).hexdigest(), 'updated': records}, indent=2))
print('Wine 11 icon label installed in source and PS5 app metadata.', flush=True)
