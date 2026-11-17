#!/usr/bin/env python3
"""Run the generated native preparation or app/kit transaction once."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import time
import urllib.request

p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['prepare','apply']);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
(root/'collab/BOARD.md').read_text(errors='replace')
assert '| console | Codex |' in (root/'collab/LOCKS.md').read_text()
folder=root/'build/vapor-backend-upgrade/install';plan=json.loads((folder/'plan.json').read_text())
assert json.loads((root/'evidence/vapor-native-updater-fixtures.json').read_text())['passed']
assert json.loads((root/'evidence/vapor-backend-kit-console-verification.json').read_text())['passed']
if a.mode=='prepare':
    assert json.loads((folder/'stage-status.json').read_text())['phase']=='uploaded-awaiting-native-copy-and-verification'
else:
    assert json.loads((root/'evidence/vapor-backend-pair-prepare.json').read_text())['passed']
    assert '100% tests passed' in (root/'build/vapor-backend-upgrade/ctest-final.log').read_text()
evidence=root/f'evidence/vapor-backend-pair-{a.mode}.json'
if evidence.exists():raise SystemExit('Inspect existing transaction evidence before another run')
data=(folder/(a.mode+'.elf')).read_bytes()
assert data.startswith(b'\x7fELF') and plan['stage'].encode() in data and plan['files']['eboot.bin'].encode() in data
processes=json.load(urllib.request.urlopen('http://192.168.7.26:8084/processes_list',timeout=5))['processes']
if any(x['name'] in ('eboot.bin','payload.elf') for x in processes):raise SystemExit('Close active titles/payloads before continuing')
result={'mode':a.mode,'payload_sha256':hashlib.sha256(data).hexdigest(),'plan_sha256':hashlib.sha256((folder/'plan.json').read_bytes()).hexdigest(),'eboot_sha256':plan['files']['eboot.bin'],'kit_manifest_sha256':plan['new_kit_sha256'],'automatic_retry':False,'phase':'starting'}
evidence.write_text(json.dumps(result,indent=2)+'\n')
output=bytearray();expected=b'VAPOR_PAIR prepared complete' if a.mode=='prepare' else b'VAPOR_PAIR installed verified'
try:
    with socket.create_connection(('192.168.7.26',9021),timeout=5) as connection:
        connection.sendall(data);connection.shutdown(socket.SHUT_WR);deadline=time.monotonic()+600
        while time.monotonic()<deadline:
            connection.settimeout(1)
            try:chunk=connection.recv(4096)
            except socket.timeout:continue
            if not chunk:break
            output.extend(chunk)
            if len(output)>32768:raise RuntimeError('Unexpected transaction output')
            if expected in output:break
    result.update(passed=expected in output,phase='completed' if expected in output else 'inspect-before-retry',output=output.decode(errors='replace'))
except Exception as error:result.update(passed=False,phase='inspect-before-retry',error=str(error),output=output.decode(errors='replace'))
evidence.write_text(json.dumps(result,indent=2)+'\n');print(result['output'])
if not result['passed']:raise SystemExit('Native transaction did not complete; inspect before another action')
