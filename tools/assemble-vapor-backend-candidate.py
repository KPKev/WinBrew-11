#!/usr/bin/env python3
"""Assemble a new, verified app tree from the native backend candidate.

Preserves the complete baseline's scoped compatibility runtimes. Produces local
review artifacts only; does not install, publish or mark the candidate tested.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[1]
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while b:=f.read(1<<20):h.update(b)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True)
p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--refresh-existing',action='store_true',help='Verify and refresh an existing local candidate without copying its baseline again')
a=p.parse_args()
if a.refresh_existing:
    a.base=a.output
baseline=json.loads((a.base/'manifest.json').read_text())
actual={f.relative_to(a.base).as_posix() for f in a.base.rglob('*') if f.is_file() and f.relative_to(a.base).as_posix()!='manifest.json'}
if actual!=set(baseline['files']) or any(f.is_symlink() for f in a.base.rglob('*')):
    raise SystemExit('Baseline file inventory changed')
for n,h in baseline['files'].items():
    if sha(a.base/n)!=h:raise SystemExit('Baseline file changed: '+n)
receipt=json.loads((a.candidate/'candidate.json').read_text())
if sha(a.candidate/'app.elf')!=receipt['sha256']:raise SystemExit('Candidate ELF changed')
if a.output.exists() and not a.refresh_existing:raise SystemExit('Use a new output directory')
if not a.refresh_existing:shutil.copytree(a.base,a.output)
for f in (a.source/'runtime').rglob('*'):
    if f.is_symlink():raise SystemExit('Runtime asset symlink')
    if f.is_file():
        target=a.output/'runtime'/f.relative_to(a.source/'runtime');target.parent.mkdir(parents=True,exist_ok=True)
        if not target.is_file() or sha(f)!=sha(target):shutil.copy2(f,target)
pie=(a.candidate/'app.elf').read_bytes()
for old,new in [(b'/mnt/sandbox/PPSA77381_000/download0',b'/mnt/sandbox/PPSA77382_000/download0'),(b'/mnt/sandbox/PPSA77381_000/app0',b'/mnt/sandbox/PPSA77382_000/app0')]:
    if pie.count(old)!=1:raise SystemExit('Unexpected native mount identity')
    pie=pie.replace(old,new)
if b'PPSA77381' in pie:raise SystemExit('Wrong native title identity remains')
(a.candidate/'pie.elf').write_bytes(pie)
tool=root/'vendor/ps5-native-app-boilerplate/build/host/ps5-native-tool'
sdk=root/'vendor/playden-source/deps/ps5-payload-sdk'
cmd=[str(tool),'link','--in',str(a.candidate/'pie.elf'),'--out',str(a.candidate/'eboot.elf'),'--stub-dir',str(sdk/'target/lib')]
for n in ('libSceCommonDialog.so','libSceMouse.so','libSceWebBrowserDialog.so'):
    cmd+=['--stub',str(root/'build/native-imports'/n)]
subprocess.run(cmd+['--module-sdk','0x02000009','--companion-sdk','0x08050001','--file-name','eboot.elf'],check=True)
subprocess.run([str(tool),'self','--sign','--in',str(a.candidate/'eboot.elf'),'--out',str(a.output/'eboot.bin'),'--magic','0x1D3D154F'],check=True)
param=json.loads((a.output/'sce_sys/param.json').read_text())
if param['titleId']!='PPSA77382':raise SystemExit('Wrong baseline title')
param['contentVersion']='01.018.000'
(a.output/'sce_sys/param.json').write_text(json.dumps(param,indent=2)+'\n')
(a.output/'playden-version').write_text('1.18.0\n')
baseline.update(version='01.018.000',backend_version='0.11.0',backend_source_revision=receipt['source'],base_kit_version='0.28',console_tested=False)
baseline['files']={f.relative_to(a.output).as_posix():sha(f) for f in sorted(a.output.rglob('*')) if f.is_file() and f.relative_to(a.output).as_posix()!='manifest.json'}
(a.output/'manifest.json').write_text(json.dumps(baseline,indent=2,sort_keys=True)+'\n')
print('PASS: assembled complete app candidate',a.output,'eboot',sha(a.output/'eboot.bin'))
