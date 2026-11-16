#!/usr/bin/env python3
"""Assemble a complete release candidate from a verified base and staged update.
Local files only. No console action, publication, accounts or user data.
"""
import argparse, hashlib, json, shutil
from pathlib import Path
import prepare_blender_runtime as blender

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def assemble(base, update, output):
    if output.exists(): raise ValueError('Output must be new')
    baseline=json.loads((update/'baseline-manifest.json').read_text())
    manifest=json.loads((update/'manifest.json').read_text())
    plan=json.loads((update/'plan.json').read_text())
    # Verify every input before creating the final tree. Update is the reviewed
    # local build; unchanged files must match the installed baseline receipt.
    sources={}
    for name, expected in manifest['files'].items():
        relative=Path(name)
        if relative.is_absolute() or '..' in relative.parts: raise ValueError('Unsafe path')
        path=(update if name in plan['paths'] else base)/name
        if path.is_symlink() or not path.is_file() or sha(path)!=expected: raise ValueError('Input differs: '+name)
        if name not in plan['paths'] and baseline['files'].get(name)!=expected: raise ValueError('Unreviewed base: '+name)
        sources[name]=path
    blender.verify()
    for name,path in blender.notice_files().items(): sources[name]=path
    output.mkdir(parents=True)
    for name,path in sources.items():
        target=output/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    manifest['files']={name:sha(output/name) for name in sorted(sources)}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    print('Verified full candidate:',output,len(sources),'files; console acceptance remains separate')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('base',type=Path);p.add_argument('update',type=Path);p.add_argument('new_output',type=Path)
    a=p.parse_args();assemble(a.base,a.update,a.new_output)
