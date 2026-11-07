#!/usr/bin/env python3
"""Restore release file modes after a PS5 file manager/FTP copy loses them.

Standard-library-only helper for the cs2-runtime-v1/v1.1 releases. Default is a
read-only plan. --apply changes modes of manifest-listed app/base-kit files
and their parent directories only; no game, account or save paths are used.
"""
import argparse, hashlib, json, time, urllib.parse, urllib.request
from pathlib import PurePosixPath

APP = '/data/homebrew/PPSA77382'
KIT = '/data/wine11/kits/playden-kit'
APP_PIN = ('933305eb4f6e1e2edb034bc92cbb6dfbc48136fb3ac0665ddefd9756382379be',
           'a712a3d69ad0eae2829415486053790ce38361156f4bab6442d2838e4a9b4c4c',
           'e245bfab1cc296be9e586bac86bd61160073b6ba99c22faf84da71c2ea203f12')
KIT_PIN = '1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc'
RUNTIME_PIN = 'd7cf3ed3221f8e8a7eb571522363efc21a5540699b6b20b3226cf30012732259'
PREFIX = 'runtime/compatibility/steam-730-cs2-v1/'
BLENDER_PREFIX = 'runtime/compatibility/steam-365670-blender-v1/'
BLENDER_PIN = '2191c6d66d0964fb3916cc797d992204729e3bcf3ac0db73f57a4630ef1f2644'

class Client:
    def __init__(self, host, port): self.base = f'http://{host}:{port}'
    def post(self, endpoint, values, query=False):
        url=self.base+endpoint+('?' + urllib.parse.urlencode(values) if query else '')
        request = urllib.request.Request(url, data=b'' if query else urllib.parse.urlencode(values).encode(),
            headers={'Content-Type':'application/x-www-form-urlencoded'}, method='POST')
        with urllib.request.urlopen(request, timeout=60) as response: result=json.load(response)
        if not result.get('ok'): raise RuntimeError(str(result))
        return result
    def read(self, path):
        task=self.post('/api/download/prepare', {'paths':path})['task_id']
        with urllib.request.urlopen(self.base+'/api/download?id='+str(task), timeout=60) as response:
            return response.read(4*1024*1024)
    def listing(self, path): return self.post('/api/list', {'path':path},query=True)['entries']
    def chmod(self, paths, mode):
        task=self.post('/api/chmod', {'paths':'\n'.join(paths),'mode':f'{mode:04o}','recursive':'0'})['task_id']
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            row=next(t for t in self.post('/api/tasks',{})['tasks'] if t['id']==task)
            if row['state'] in ('done','completed'): return
            if row['state'] not in ('running','pending','queued'): raise RuntimeError(str(row))
            time.sleep(.5)
        raise RuntimeError('Completion unknown; inspect the file-manager task before retrying')

def document(raw, pin):
    if hashlib.sha256(raw).hexdigest() not in (pin if isinstance(pin, tuple) else (pin,)): raise ValueError('Installed manifest differs from this release; no permission changes')
    return json.loads(raw)

def paths_and_directories(root, modes):
    result={root:0o755}
    for name, mode in modes.items():
        path=PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name or str(path)!=name:
            raise ValueError('Unsafe manifest path')
        result[root+'/'+name]=mode
        for parent in path.parents:
            if str(parent)!='.': result[root+'/'+str(parent)]=0o755
    return result

def plan(client, include_kit=False):
    app=document(client.read(APP+'/manifest.json'),APP_PIN)
    runtime=document(client.read(APP+'/'+PREFIX+'vapor-runtime.json'),RUNTIME_PIN)
    modes={name:0o644 for name in app['files']}; modes['manifest.json']=0o644
    for name in ['eboot.bin','lapy.elf','vapor-title-remover.elf']: modes[name]=0o755
    for entry in runtime['files']:
        name=PREFIX+entry['path']
        if app['files'].get(name)!=entry['sha256']: raise ValueError('Runtime binding differs')
        modes[name]=int(entry['mode'],8)
    if BLENDER_PREFIX+'vapor-runtime.json' in app['files']:
        blender=document(client.read(APP+'/'+BLENDER_PREFIX+'vapor-runtime.json'),BLENDER_PIN)
        for entry in blender['files']+blender['title_files']:
            name=BLENDER_PREFIX+entry['path']
            if app['files'].get(name)!=entry['sha256']: raise ValueError('Blender runtime binding differs')
            modes[name]=int(entry['mode'],8)
    result=paths_and_directories(APP,modes)
    if include_kit:
        kit=document(client.read(KIT+'/manifest.json'),KIT_PIN)
        modes={f['path']:int(f['mode'],8) for f in kit['files']}; modes['manifest.json']=0o644
        result.update(paths_and_directories(KIT,modes))
    if any(mode not in (0o644,0o755) for mode in result.values()): raise ValueError('Unexpected mode')
    return result

def inspect(client, planned):
    listings={}
    changed={}
    # Check every named entry before doing any write; unrelated files are ignored.
    for name, mode in planned.items():
        path=PurePosixPath(name);parent=str(path.parent)
        if parent not in listings: listings[parent]={e['name']:e for e in client.listing(parent)}
        entry=listings[parent].get(path.name)
        if not entry or entry['type'] not in ('d','-'): raise ValueError('Missing or linked path: '+name)
        if entry['mode'] & 0o7777 != mode: changed[name]=mode
    return changed

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',required=True,help='PS5 IP address with Web File Manager running')
    parser.add_argument('--port',type=int,default=8888)
    parser.add_argument('--base-kit',action='store_true',help='also fix the dedicated base-kit modes for a new setup')
    parser.add_argument('--apply',action='store_true',help='apply the printed plan')
    args=parser.parse_args();client=Client(args.host,args.port)
    planned=plan(client,args.base_kit);changed=inspect(client,planned)
    print(f'Verified release manifests; {len(changed)} of {len(planned)} packaged paths need mode changes.')
    if args.apply:
        for mode in (0o755,0o644):
            paths=sorted(name for name,value in changed.items() if value==mode)
            for index in range(0,len(paths),100): client.chmod(paths[index:index+100],mode)
        if inspect(client,planned): raise RuntimeError('Some modes remain incorrect; inspect before retrying')
        print('PASS: packaged file modes verified; no file contents, games, accounts or saves changed.')
    else: print('Read-only plan. Add --apply to restore these release modes.')
