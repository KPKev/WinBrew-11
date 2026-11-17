#!/usr/bin/env python3
"""Derive the reviewed Vapor backend from an exact, pristine Playden checkout.

Run under the repository build/git locks. Never modifies either vendor checkout.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--upstream', type=Path, default=root/'vendor/playden-backend')
parser.add_argument('--output', type=Path, default=root/'build/vapor-backend-source')
args = parser.parse_args()
recipe = root/'runtime-patches/playden-0.11'
spec = json.loads((recipe/'source.json').read_text())
patch = recipe/'vapor.patch'
if hashlib.sha256(patch.read_bytes()).hexdigest() != spec['patch_sha256']:
    raise SystemExit('Reviewed backend patch changed; review and regenerate its receipt.')
revision = subprocess.check_output(['git','-C',str(args.upstream),'rev-parse','HEAD'], text=True).strip()
if revision != spec['upstream']:
    raise SystemExit('Unexpected Playden revision')
if subprocess.check_output(['git','-C',str(args.upstream),'diff','--name-only','HEAD'], text=True).strip():
    raise SystemExit('Playden checkout has local changes')
if args.output.exists():
    raise SystemExit('Output already exists; choose a fresh build directory')
for name in ('src','cmake','runtime','fixtures','tests','tools','scripts'):
    shutil.copytree(args.upstream/name,args.output/name)
for name in ('CMakeLists.txt','dependencies.lock.json'):
    shutil.copy2(args.upstream/name,args.output/name)
# All portable third-party sources retain their original pinned hashes. Native
# network archive hashes in the patch refer to our separately qualified SDK build.
shutil.copytree(root/'vendor/playden-source/third_party',args.output/'third_party')
subprocess.run(['patch','--batch','--fuzz=0','-p1','-i',str(patch)],cwd=args.output,check=True)
integration = root/'src/steam_integration'
for source in integration.glob('*.hpp'):
    shutil.copy2(source,args.output/'src'/source.name)
    (args.output/'src/steam_integration').mkdir(exist_ok=True)
    shutil.copy2(source,args.output/'src/steam_integration'/source.name)
shutil.copy2(integration/'runtime_install_id.cpp',args.output/'src/runtime_install_id.cpp')
(args.output/'src/vapor_backend_version.h').write_text('#pragma once\n#define WW_PLAYDEN_BACKEND_VERSION '+json.dumps(spec['version'])+'\n')
(args.output/'vapor-source.json').write_text(json.dumps(spec,indent=2)+'\n')
print('Prepared Vapor backend',spec['version'],'at',args.output)
