"""Verify the owner's manual old-title uninstall, then remove only its leftovers."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import uuid
import zipfile
from install_console import WFM, ROOT

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--apply', action='store_true')
args = parser.parse_args()
w = WFM('192.168.7.26', 8889)
snapshot = ROOT / 'build' / ('registration-' + uuid.uuid4().hex[:10])
snapshot.mkdir()
entries = {e['name'] for e in w.listing('/system_data/priv/mms')}
for name in ('app.db', 'app.db-wal'):
    if name in entries:
        (snapshot / name).write_bytes(w.read('/system_data/priv/mms/' + name))
db = sqlite3.connect('file:' + str((snapshot / 'app.db').resolve()) + '?mode=ro', uri=True)
rows = db.execute('select titleId,titleName from tbl_contentinfo where titleId in (?,?)',
                  ('PPSA77380', 'PPSA77381')).fetchall()
print('App database registrations:', rows)
if any(row[0] == 'PPSA77380' for row in rows):
    raise RuntimeError('The old application is still registered; no source deletion performed')
if not any(row[0] == 'PPSA77381' and row[1] == 'Wine 11' for row in rows):
    raise RuntimeError('New Wine 11 registration is not confirmed')
backup = ROOT / 'build/retired-WrappedWine-PPSA77380.zip'
with zipfile.ZipFile(backup) as archive:
    assert archive.testzip() is None
    hashes = {name: hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()}
old = '/data/homebrew/PPSA77380'
present = any(e['name'] == 'PPSA77380' for e in w.listing('/data/homebrew'))
if present:
    found = {}
    def walk(path, prefix=''):
        for entry in w.listing(path):
            name = prefix + entry['name']
            if entry['type'] == 'd':
                walk(path + '/' + entry['name'], name + '/')
            elif entry['type'] == '-':
                found[name] = hashlib.sha256(w.read(path + '/' + entry['name'])).hexdigest()
            else:
                raise RuntimeError('Unexpected link/device in old source')
    walk(old)
    if found != hashes:
        raise RuntimeError('Old source differs from its verified backup; inspect before cleanup')
    if not args.apply:
        print('Verified old source and backup; use --apply for exact-folder cleanup')
        raise SystemExit(0)
    assert old == '/data/homebrew/PPSA77380'
    task = w.post('/api/delete', {'paths': old})['task_id']
    w.wait(task)
assert not any(e['name'] == 'PPSA77380' for e in w.listing('/data/homebrew'))
(ROOT / 'evidence/wine11-replacement.json').write_text(json.dumps({
    'removed_title': 'PPSA77380', 'removed_name': 'WrappedWine',
    'installed_title': 'PPSA77381', 'installed_name': 'Wine 11',
    'old_registration_absent': True, 'old_source_absent': True,
    'native_uninstall_method': 'owner PS5 Options Delete',
    'backup_files': hashes, 'four_pane_design_preserved': True, 'icon_label': 'Wine 11'}, indent=2))
print('Verified: old registration and old source absent; Wine 11 remains registered.')
