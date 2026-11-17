#!/usr/bin/env python3
"""Run the disposable-file updater regression on the quiescent console once.

The compiled fixture uses random /data/vapor-backend-update-test-* folders and
an in-memory fake release server. It never names the live app or runtime roots.
"""
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

root=Path(__file__).resolve().parents[1]
(root/'collab/BOARD.md').read_text(errors='replace')
if '| console | Codex |' not in (root/'collab/LOCKS.md').read_text():raise SystemExit('Console lock required')
evidence=root/'evidence/vapor-native-updater-fixtures.json'
if evidence.exists():raise SystemExit('Inspect the previous test before another run')
binary=root/'build/vapor-backend-upgrade/native-tests/test.elf'
data=binary.read_bytes()
if not data.startswith(b'\x7fELF') or b'/data/vapor-backend-update-test-' not in data:
    raise SystemExit('Expected disposable-fixture test ELF')
processes=json.load(urllib.request.urlopen('http://192.168.7.26:8084/processes_list',timeout=5))['processes']
if any(p['name'] in ('eboot.bin','payload.elf') for p in processes):raise SystemExit('Console has an active title or payload')
state={'sha256':hashlib.sha256(data).hexdigest(),'live_app_modified':False,'live_kit_modified':False,'automatic_retry':False,'phase':'starting'}
evidence.write_text(json.dumps(state,indent=2)+'\n')
output=bytearray()
try:
    with socket.create_connection(('192.168.7.26',9021),timeout=5) as connection:
        connection.sendall(data);connection.shutdown(socket.SHUT_WR)
        deadline=time.monotonic()+180
        while time.monotonic()<deadline:
            connection.settimeout(1)
            try:chunk=connection.recv(4096)
            except socket.timeout:continue
            if not chunk:break
            output.extend(chunk)
            if len(output)>32768:raise RuntimeError('Unexpected test output volume')
            if b'Vapor backend update transactions passed' in output:break
    state['output']=output.decode(errors='replace')
    state['passed']='Vapor backend update transactions passed' in state['output']
    state['phase']='completed' if state['passed'] else 'inspect-before-retry'
except Exception as error:
    state.update(phase='inspect-before-retry',error=str(error),output=output.decode(errors='replace'),passed=False)
evidence.write_text(json.dumps(state,indent=2)+'\n')
print(state.get('output',''))
if not state.get('passed'):raise SystemExit('Native updater fixture test did not pass; inspect evidence and process state')
