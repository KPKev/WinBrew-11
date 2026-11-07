"""Update only the registered WinBrew 11 app icon, with a verified backup."""
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request

from install_console import ROOT, WFM

PARENT = '/user/app/PPSA77382/sce_sys'
OLD_HASH = 'aa3ce89ea34ee3c3e53307a4ab890317aaea14fcc546ad2dc8aafa7c43703fec'
NEW_HASH = '65ebb2f88882831091c09f4de19d12fe24415ef82773e21889e1f34d1b8590af'
TAG = 'winbrew11-home-icon'
STATUS = ROOT / 'build/winbrew11-home-icon-status.json'
BACKUP = ROOT / 'build/winbrew11-home-icon-before.png'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require_stopped():
    with urllib.request.urlopen('http://192.168.7.26:8084/processes_list', timeout=5) as response:
        processes = json.load(response).get('processes')
    if not isinstance(processes, list) or any(str(item.get('name', '')).lower() == 'eboot.bin' for item in processes):
        raise RuntimeError('Close running applications before changing the icon')


def main():
    if STATUS.exists() or BACKUP.exists():
        raise RuntimeError('Existing journal or backup: inspect before retrying')
    require_stopped()
    wfm = WFM('192.168.7.26')
    metadata = json.loads(wfm.read(PARENT + '/param.json'))
    if metadata.get('titleId') != 'PPSA77382' or metadata['localizedParameters']['en-US']['titleName'] != 'WinBrew 11':
        raise RuntimeError('Registered title identity differs')
    original = wfm.read(PARENT + '/icon0.png')
    replacement = (ROOT / 'build/winbrew11-isolated-v2/PPSA77382/sce_sys/icon0.png').read_bytes()
    if digest(original) != OLD_HASH or digest(replacement) != NEW_HASH:
        raise RuntimeError('Icon hashes differ from the reviewed originals')
    if digest(wfm.read('/data/homebrew/PPSA77382/sce_sys/icon0.png')) != NEW_HASH:
        raise RuntimeError('WinBrew source icon differs')
    if digest(wfm.read('/user/appmeta/PPSA77382/icon0.png')) != NEW_HASH:
        raise RuntimeError('WinBrew metadata icon differs')
    stage = 'icon0.png.' + TAG + '.stage'
    saved = 'icon0.png.before-' + TAG + '.bak'
    entries = {item['name'] for item in wfm.listing(PARENT)}
    if stage in entries or saved in entries:
        raise RuntimeError('Prior staging or backup exists: inspect before retrying')
    BACKUP.write_bytes(original)
    if digest(BACKUP.read_bytes()) != OLD_HASH:
        raise RuntimeError('Local backup verification failed')
    state = {'phase': 'staging', 'title': 'PPSA77382', 'original_sha256': OLD_HASH, 'new_sha256': NEW_HASH}
    STATUS.write_text(json.dumps(state, indent=2) + '\n')
    try:
        task = wfm.post('/api/upload/prepare', {'path': PARENT, 'src': stage, 'total': str(len(replacement)),
                        'count': '1', 'rels': stage, 'sizes': str(len(replacement)), 'overwrite': '0'})['task_id']
        wfm.post('/api/upload-file', replacement, headers={'Content-Type': 'application/octet-stream',
                 'X-WFM-Task-ID': str(task), 'X-WFM-Path': urllib.parse.quote(PARENT, safe=''),
                 'X-WFM-Rel': urllib.parse.quote(stage, safe=''), 'X-WFM-Size': str(len(replacement)), 'X-WFM-Overwrite': '0'})
        wfm.post('/api/upload/finish', params={'task_id': task})
        wfm.wait(task)
        if digest(wfm.read(PARENT + '/' + stage)) != NEW_HASH:
            raise RuntimeError('Staged icon differs')
        task = wfm.post('/api/chmod', {'paths': PARENT + '/' + stage, 'mode': '0666', 'recursive': '0'})['task_id']
        wfm.wait(task)
        require_stopped()
        if digest(wfm.read(PARENT + '/icon0.png')) != OLD_HASH:
            raise RuntimeError('Original icon changed before promotion')
        state['phase'] = 'promoting'
        STATUS.write_text(json.dumps(state, indent=2) + '\n')
        wfm.post('/api/rename', params={'path': PARENT + '/icon0.png', 'name': saved})
        state['phase'] = 'original-backed-up'
        STATUS.write_text(json.dumps(state, indent=2) + '\n')
        try:
            wfm.post('/api/rename', params={'path': PARENT + '/' + stage, 'name': 'icon0.png'})
        except Exception:
            wfm.post('/api/rename', params={'path': PARENT + '/' + saved, 'name': 'icon0.png'})
            raise
        if digest(wfm.read(PARENT + '/icon0.png')) != NEW_HASH:
            raise RuntimeError('Promoted icon differs')
        state['phase'] = 'verified'
        STATUS.write_text(json.dumps(state, indent=2) + '\n')
        print('Installed-app icon read-back verified; Home screen refresh still needs visual confirmation.')
    except Exception as error:
        state.update(phase='inspect-before-retry', error=str(error))
        STATUS.write_text(json.dumps(state, indent=2) + '\n')
        raise


if __name__ == '__main__':
    main()
