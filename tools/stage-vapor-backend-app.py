#!/usr/bin/env python3
"""Upload the finite app delta into a fresh, inactive console staging folder."""
import hashlib
import json
from pathlib import Path
import re
import urllib.parse
from install_console import WFM

root=Path(__file__).resolve().parents[1]
(root/'collab/BOARD.md').read_text(errors='replace')
assert '| console | Codex |' in (root/'collab/LOCKS.md').read_text()
folder=root/'build/vapor-backend-upgrade/install'
plan=json.loads((folder/'plan.json').read_text())
stage=plan['stage'];assert re.fullmatch(r'/data/vapor-backend-app-stage-[0-9a-f]{12}',stage)
status=folder/'stage-status.json'
if status.exists():raise SystemExit('Inspect existing stage before another upload')
w=WFM('192.168.7.26')
def sha(data):return hashlib.sha256(data).hexdigest()
assert sha(w.read('/data/homebrew/PPSA77382/manifest.json'))==plan['old_manifest_sha256']
assert stage.rsplit('/',1)[1] not in {x['name'] for x in w.listing('/data')}
files=plan['changed'];source=Path(plan['app_source'])
for name in files:
    assert not Path(name).is_absolute() and '..' not in Path(name).parts
    assert sha((source/name).read_bytes())==plan['files'][name]
state={'phase':'starting','stage':stage,'live_app_modified':False,'live_kit_modified':False,'automatic_retry':False}
def mark(phase,**fields):
    state.update(phase=phase,**fields);status.write_text(json.dumps(state,indent=2)+'\n');print(phase,flush=True)
with status.open('x') as f:json.dump(state,f)
try:
    w.post('/api/mkdir',params={'path':'/data','name':stage.rsplit('/',1)[1]})
    w.post('/api/mkdir',params={'path':stage,'name':'incoming'})
    target=stage+'/incoming';made={''}
    for name in files:
        parent=''
        for part in name.split('/')[:-1]:
            child=(parent+'/'+part).lstrip('/')
            if child not in made:
                w.post('/api/mkdir',params={'path':target+('/'+parent if parent else ''),'name':part});made.add(child)
            parent=child
    sizes=[(source/n).stat().st_size for n in files]
    task=w.post('/api/upload/prepare',{'path':target,'src':files[0],'total':str(sum(sizes)),'count':str(len(files)),'rels':'\n'.join(files),'sizes':'\n'.join(map(str,sizes)),'overwrite':'0'})['task_id']
    mark('uploading',task_id=task,files=len(files),bytes=sum(sizes))
    for index,(name,size) in enumerate(zip(files,sizes)):
        data=(source/name).read_bytes()
        w.post('/api/upload-file',data,headers={'Content-Type':'application/octet-stream','X-WFM-Task-ID':str(task),'X-WFM-Path':urllib.parse.quote(target,safe=''),'X-WFM-Rel':urllib.parse.quote(name,safe=''),'X-WFM-Size':str(size),'X-WFM-Overwrite':'0'})
        if (index+1)%16==0:mark('uploading',uploaded_files=index+1)
    w.post('/api/upload/finish',params={'task_id':task});w.wait(task)
    mark('uploaded-awaiting-native-copy-and-verification',task_id=None)
except Exception as error:
    mark('inspect-before-retry',error=str(error));raise
