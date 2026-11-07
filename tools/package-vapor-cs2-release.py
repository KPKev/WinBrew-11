#!/usr/bin/env python3
"""Create a distributable Vapor app ZIP with verified bytes and Unix modes."""
import argparse, hashlib, json, zipfile
from pathlib import Path
import prepare_cs2_runtime as cs2
import prepare_blender_runtime as blender

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def package(app, output):
    app, output = Path(app), Path(output)
    manifest = json.loads((app/'manifest.json').read_bytes())
    if json.loads((app/'sce_sys/param.json').read_bytes())['titleId'] != 'PPSA77382':
        raise ValueError('Unexpected app title')
    actual = {p.relative_to(app).as_posix() for p in app.rglob('*') if p.is_file() and p.name != 'manifest.json'}
    if any(p.is_symlink() for p in app.rglob('*')) or actual != set(manifest['files']):
        raise ValueError('App inventory differs')
    for name, expected in manifest['files'].items():
        if sha(app/name) != expected: raise ValueError('App file differs: ' + name)
    cs2.check_header()
    for name, expected in cs2.expected_hashes().items():
        if manifest['files'].get(name) != expected: raise ValueError('Pinned runtime or notices differ: ' + name)
    blender.check_header()
    for name, expected in blender.expected_hashes().items():
        if manifest['files'].get(name) != expected: raise ValueError('Pinned Blender runtime or notices differ: ' + name)
    modes = {cs2.PREFIX+f['path']: int(f['mode'],8) for f in cs2.entries()}
    modes.update({blender.PREFIX+f['path']: int(f['mode'],8) for f in blender.entries()})
    modes.update({name:0o755 for name in ['eboot.bin','lapy.elf','vapor-title-remover.elf']})
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as bundle:
        paths=[app]+sorted(app.rglob('*'))
        for path in paths:
            rel=path.relative_to(app).as_posix(); rel='' if rel=='.' else rel
            name='PPSA77382/'+rel+('/' if path.is_dir() and rel else '')
            info=zipfile.ZipInfo(name,(2026,11,7,0,0,0)); info.create_system=3
            info.external_attr=(0o40755 if path.is_dir() else 0o100000|modes.get(rel,0o644))<<16
            info.compress_type=zipfile.ZIP_DEFLATED
            bundle.writestr(info,b'' if path.is_dir() else path.read_bytes())
        bundle.writestr('INSTALL.txt',(cs2.ROOT/'docs/cs2-runtime.md').read_bytes())
        bundle.writestr('BLENDER.txt',(cs2.ROOT/'docs/blender-runtime.md').read_bytes())
        bundle.writestr('RELEASE-NOTES.txt',(cs2.ROOT/'docs/release-notes-next.md').read_bytes())
    print('PASS:',len(manifest['files']),'verified app files; ZIP SHA256',sha(output))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('app'); parser.add_argument('new_output_zip')
    args=parser.parse_args();package(args.app,args.new_output_zip)
