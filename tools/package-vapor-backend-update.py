#!/usr/bin/env python3
"""Package a matched Vapor app and runtime for its in-app updater.

Candidates remain non-installable through the public updater. A qualification
receipt must bind both exact app and kit hashes before an installable manifest
can be emitted. This script never publishes a release or contacts a console.
"""
import argparse
import hashlib
import json
from pathlib import Path
import stat
import zipfile
import prepare_cs2_runtime as cs2
import prepare_blender_runtime as blender

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        while chunk:=stream.read(1<<20): h.update(chunk)
    return h.hexdigest()

def package(app,kit,out,qualification=None):
    manifest=json.loads((app/'manifest.json').read_text())
    param=json.loads((app/'sce_sys/param.json').read_text())
    if param['titleId']!='PPSA77382' or param['contentVersion']!='01.018.000':
        raise ValueError('Unexpected candidate title/version')
    files={p.relative_to(app).as_posix():p for p in app.rglob('*') if p.is_file() and p.relative_to(app).as_posix()!='manifest.json'}
    if set(files)!=set(manifest['files']) or any(p.is_symlink() for p in app.rglob('*')):
        raise ValueError('App inventory differs')
    for name,path in files.items():
        if sha(path)!=manifest['files'][name]: raise ValueError('Changed app file: '+name)
    km=json.loads((kit/'manifest.json').read_text())
    entries={e['path']:e for e in km['files']}
    actual={p.relative_to(kit).as_posix():p for p in kit.rglob('*') if p.is_file() and p.relative_to(kit).as_posix()!='manifest.json'}
    if set(actual)!=set(entries) or any(p.is_symlink() for p in kit.rglob('*')):
        raise ValueError('Kit inventory differs')
    for name,path in actual.items():
        e=entries[name]
        if sha(path)!=e['sha256'] or path.stat().st_size!=e['size'] or stat.S_IMODE(path.stat().st_mode)!=int(e['mode'],8):
            raise ValueError('Changed kit file: '+name)
    tested=False
    if qualification:
        q=json.loads(qualification.read_text())
        if q.get('eboot_sha256')!=sha(app/'eboot.bin') or q.get('kit_manifest_sha256')!=sha(kit/'manifest.json') or q.get('firmware')!='13.60' or q.get('startup_verified') is not True or q.get('update_flow_verified') is not True:
            raise ValueError('Qualification does not cover these exact app/kit bytes')
        tested=True
    out.mkdir(parents=True,exist_ok=False)
    app_name='WinBrew-11-v1.18.0.zip';kit_name='Vapor-backend-kit-0.28.zip'
    def archive(path,base,prefix,modes):
        with zipfile.ZipFile(path,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as bundle:
            for source in sorted(base.rglob('*')):
                if not source.is_file(): continue
                name=source.relative_to(base).as_posix()
                info=zipfile.ZipInfo(prefix+name,(2026,11,16,0,0,0));info.create_system=3
                info.external_attr=(0o100000|modes.get(name,0o644))<<16;info.compress_type=zipfile.ZIP_DEFLATED
                bundle.writestr(info,source.read_bytes())
    modes={n:0o755 for n in files if n in ('eboot.bin','lapy.elf','vapor-title-remover.elf') or n.startswith('sce_module/')}
    for policy in (cs2,blender):
        policy.check_header()
        for name,expected in policy.expected_hashes().items():
            if manifest['files'].get(name)!=expected: raise ValueError('Changed compatibility runtime: '+name)
        modes.update({policy.PREFIX+e['path']:int(e['mode'],8) for e in policy.entries()})
    archive(out/app_name,app,'PPSA77382/',modes)
    archive(out/kit_name,kit,'vapor-kit-0.28/playden-kit/',{n:int(e['mode'],8) for n,e in entries.items()})
    # Keep the official release's source and license notices with the derived kit.
    notices=Path(__file__).resolve().parents[1]/'build/playden-kit-0.28.zip'
    if sha(notices)!='9287fc92e1cd22dac3c09ded274c8e0a58ba1609ceab5b609265da9ab4caf424':
        raise ValueError('Upstream runtime notices archive changed')
    with zipfile.ZipFile(notices) as original, zipfile.ZipFile(out/kit_name,'a',zipfile.ZIP_DEFLATED) as bundle:
        for entry in original.infolist():
            if not entry.is_dir() and (entry.filename.startswith('playden-kit-0.28/LICENSES/') or entry.filename.endswith('/SOURCES.md')):
                info=zipfile.ZipInfo('release-notices/'+entry.filename.split('/',1)[1],(2026,11,16,0,0,0))
                info.create_system=3;info.external_attr=0o100644<<16;info.compress_type=zipfile.ZIP_DEFLATED
                bundle.writestr(info,original.read(entry))
    contract={'format':1,'app':{'version':'1.18.0','title_id':'PPSA77382','min_from':'1.17.0','asset':app_name,'prefix':'PPSA77382/'},
              'kit':{'kit_id':'playden-kit','revision':km['runtime']['artifact_revision'],'manifest_sha256':sha(kit/'manifest.json'),'asset':kit_name,'prefix':'vapor-kit-0.28/playden-kit/'},
              'notes':'Playden 0.11 backend, runtime kit 0.28, automatic update checks and preparation progress.',
              'vapor_backend':{'schema':1,'frontend_abi':1,'source_revision':'7d30aaa7dc3011561eae728badc133a6bcc7f73b','upstream_version':'0.11.0','runtime_revision':km['runtime']['artifact_revision'],'package_kind':'complete-winbrew','qualification':'ps5-13.60-reviewed' if tested else 'candidate-not-console-tested'}}
    (out/'vapor-backend-update.json').write_text(json.dumps(contract,indent=2)+'\n')
    hashes={p.name:{'sha256':sha(p),'bytes':p.stat().st_size} for p in out.iterdir() if p.is_file()}
    (out/'package-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
    print('Created matched '+('qualified update' if tested else 'non-installable qualification candidate')+': '+str(out))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--app',required=True,type=Path);p.add_argument('--kit',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--qualification',type=Path)
    a=p.parse_args();package(a.app,a.kit,a.output,a.qualification)
