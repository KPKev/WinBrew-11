#!/usr/bin/env python3
"""Package verified runtime corresponding source; excludes console and user data."""
from pathlib import Path
import argparse,hashlib,json,zipfile
ROOT=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def package(previous,output):
    root=ROOT/'runtime-patches/blender'
    receipt=json.loads((root/'source-receipt.json').read_text())
    payload={}
    with zipfile.ZipFile(previous) as z:
        for name,spec in receipt['files'].items():
            data=z.read(name) if name.startswith('archives/') else (ROOT/'runtime-patches'/name).read_bytes()
            if len(data)!=spec['size'] or sha(data)!=spec['sha256']:raise ValueError('Source differs: '+name)
            payload[name]=data
    for name in ['source-receipt.json','automatic-bridge-source-receipt.json']:
        payload['blender/'+name]=(root/name).read_bytes()
    auto=json.loads((root/'automatic-bridge-source-receipt.json').read_text())
    for name,spec in auto['files'].items():
        data=(root/name).read_bytes()
        if len(data)!=spec['size'] or sha(data)!=spec['sha256']:raise ValueError('Bridge source differs: '+name)
        payload['blender/'+name]=data
    for path in sorted((root/'upstream-notices').rglob('*')):
        if path.is_file():payload['blender/upstream-notices/'+path.relative_to(root/'upstream-notices').as_posix()]=path.read_bytes()
    payload['README.md']=b'Extract the blender/, cs2/ and archives/ directories under runtime-patches/ in the matching WinBrew source checkout. Dependency archives remain pinned; follow the source-build prerequisites.\n\n'+(ROOT/'docs/blender-runtime.md').read_bytes()
    payload['ENGINEERING-REPORT.md']=(ROOT/'docs/reports/blender-automatic-integration-20261116.md').read_bytes()
    payload['SOURCE-BUILD.md']=(ROOT/'docs/source-build.md').read_bytes()
    # Required base-runtime patch/build instructions retained alongside upstream archives.
    for path in sorted((ROOT/'runtime-patches/cs2').rglob('*')):
        if path.is_file() and not path.is_symlink() and 'build' not in path.relative_to(ROOT/'runtime-patches/cs2').parts:
            payload['cs2/'+path.relative_to(ROOT/'runtime-patches/cs2').as_posix()]=path.read_bytes()
    payload['SHA256SUMS.txt']=''.join(sha(data)+'  '+name+'\n' for name,data in sorted(payload.items())).encode()
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,data in sorted(payload.items()):
            info=zipfile.ZipInfo(name,(2026,11,16,0,0,0));info.create_system=3;info.external_attr=(0o100755 if name.endswith('.sh') else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
    print('Verified corresponding source',len(payload),'files',sha(output.read_bytes()))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('previous_source_archive',type=Path);p.add_argument('new_output',type=Path);a=p.parse_args();package(a.previous_source_archive,a.new_output)
