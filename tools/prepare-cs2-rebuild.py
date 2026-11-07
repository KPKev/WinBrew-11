#!/usr/bin/env python3
"""Stage the released corresponding source into a new POSIX rebuild workspace."""
import argparse, hashlib, io, json, os, shutil, tarfile, zipfile
from pathlib import Path, PurePosixPath

PIN = '2dd8589419f115f9d894daa1ca3492bf282cd4a0d80ad450e736671fa1e49fb5'
def prepare(bundle, destination, sdk):
    bundle, destination, sdk = Path(bundle), Path(destination).absolute(), Path(sdk).resolve(strict=True)
    if os.name != 'posix': raise ValueError('Run in Linux/WSL with the Linux PS5 SDK')
    if destination.exists(): raise ValueError('Rebuild workspace must be new')
    if not (sdk/'bin/prospero-clang').is_file(): raise ValueError('PS5 SDK is missing')
    if hashlib.sha256(bundle.read_bytes()).hexdigest() != PIN: raise ValueError('Source bundle differs')
    destination.mkdir(parents=True)
    repair=destination/'build/counterstrike-ntdll-build'; repair.mkdir(parents=True)
    evidence=destination/'evidence'; evidence.mkdir()
    with zipfile.ZipFile(bundle) as source:
        records=json.loads(source.read('sources.json'))
        for record in records:
            leaf=record['path'].replace('\\','/').split('/')[-1]
            raw=source.read('archives/'+leaf)
            if hashlib.sha256(raw).hexdigest()!=record['sha256']: raise ValueError('Source archive differs')
            (evidence/leaf).write_bytes(raw)
        (evidence/'cs2-source-archives.json').write_text(json.dumps(records[:2]))
        for item in source.infolist():
            path=PurePosixPath(item.filename)
            if path.parts[0] != 'repair' or item.is_dir(): continue
            if len(path.parts)!=2 or '..' in path.parts: raise ValueError('Unsafe repair source path')
            raw=source.read(item)
            if path.suffix in ('.sh','.py','.h','.c','.patch','.md'):
                raw=raw.replace(b'\r\r\n',b'\n').replace(b'\r\n',b'\n')
            (repair/path.name).write_bytes(raw)
    foundation=destination/'build/counterstrike-ntdll-foundation'; foundation.mkdir()
    with tarfile.open(evidence/'foundation-305975-source.tar') as archive:
        archive.extractall(foundation, filter='data')
    sdk_link=destination/'build/return-wrapper/foundation/.deps/native/ps5-payload-sdk'
    sdk_link.parent.mkdir(parents=True); sdk_link.symlink_to(sdk, target_is_directory=True)
    print('Prepared',destination)
    print('Run from that directory, with LLVM 18.1.8 at ~/.cache/wine11-llvm-18.1.8/root/usr:')
    for script in ['prepare-source.py','prepare-tools.sh','build-tool-and-test.sh','build-host-tools.sh','apply-fix.py','build-ntdll-unix.sh','link-ntdll-prx.sh','audit-ntdll.py']:
        print(('python3' if script.endswith('.py') else 'bash')+' build/counterstrike-ntdll-build/'+script)
    print('The scripts use ~/counterstrike-wine019-work, which must not contain a prior build.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('source_bundle'); p.add_argument('new_workspace'); p.add_argument('--sdk',required=True)
    a=p.parse_args(); prepare(a.source_bundle,a.new_workspace,a.sdk)
