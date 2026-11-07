"""Add a temporary PS5 logging configuration to Skyrim's title only.

Refuses existing config or an active title and never changes another game.
"""
import hashlib
import json
import socket
import urllib.parse
import urllib.request
from pathlib import Path
from install_console import WFM
from probe_ps5_kit_extraction import post, wait

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / 'build/skyrim-crash-investigation/diagnostic-config.json'
TITLE = '/data/homebrew/FAKE78005'


def main():
    if STATE.exists(): raise RuntimeError('Inspect the prior diagnostic state before retrying')
    with urllib.request.urlopen('http://192.168.7.26:8084/processes_list', timeout=5) as r:
        if any(str(p.get('name', '')).lower() == 'eboot.bin' for p in json.load(r).get('processes', [])):
            raise RuntimeError('Close the running PS5 app before staging diagnostics')
    w = WFM('192.168.7.26')
    descriptor = json.loads(w.read(TITLE + '/game-launch.json'))
    if descriptor.get('title_id') != 'FAKE78005' or descriptor.get('app_id') != 489830:
        raise RuntimeError('Skyrim title identity differs')
    if any(e['name'] == 'dev.conf' for e in w.listing(TITLE)):
        raise RuntimeError('Existing title logging configuration must be preserved')
    route = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        route.connect(('192.168.7.26', 8888))
        address = route.getsockname()[0]
    finally:
        route.close()
    data = f'DEV_SERVER={address}\nDEV_PORT=9300\nDEV_ENABLED=1\nDEV_TAG=skyrim-diagnostic\n'.encode()
    state = {'phase': 'staging', 'sha256': hashlib.sha256(data).hexdigest()}
    STATE.write_text(json.dumps(state, indent=2) + '\n')
    try:
        task = post('/api/upload/prepare', {'path': TITLE, 'src': 'dev.conf', 'total': str(len(data)), 'count': '1', 'rels': 'dev.conf', 'sizes': str(len(data)), 'overwrite': '0'})['task_id']
        post('/api/upload-file', data, headers={'Content-Type': 'application/octet-stream', 'X-WFM-Task-ID': str(task), 'X-WFM-Path': urllib.parse.quote(TITLE, safe=''), 'X-WFM-Rel': 'dev.conf', 'X-WFM-Size': str(len(data)), 'X-WFM-Overwrite': '0'})
        post('/api/upload/finish', params={'task_id': task}); wait(task)
        if hashlib.sha256(w.read(TITLE + '/dev.conf')).hexdigest() != state['sha256']:
            raise RuntimeError('Diagnostic config read-back differs')
        task = post('/api/chmod', {'paths': TITLE + '/dev.conf', 'mode': '0644', 'recursive': '0'})['task_id']; wait(task)
        state['phase'] = 'ready'; STATE.write_text(json.dumps(state, indent=2) + '\n')
        print('Temporary Skyrim-only diagnostic configuration verified.')
    except Exception as exc:
        state.update(phase='inspect-before-retry', error=str(exc)); STATE.write_text(json.dumps(state, indent=2) + '\n')
        raise


if __name__ == '__main__': main()
