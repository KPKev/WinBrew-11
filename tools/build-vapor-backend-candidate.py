#!/usr/bin/env python3
"""Compile and link Vapor's complete frontend against one derived backend.

Requires a fresh tools/build-steam-native-ui-probe.sh snapshot and a configured
native backend build. Output is a qualification candidate, never auto-deployed.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',required=True,type=Path)
p.add_argument('--native-build',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
args=p.parse_args()
source=args.source.resolve(); native_build=args.native_build.resolve(); out=args.output.resolve()
out.mkdir(parents=True,exist_ok=True)
sdk=root/'vendor/playden-source/deps/ps5-payload-sdk'
llvm=Path(os.environ.get('WINE11_LLVM_CACHE',Path.home()/'.cache/wine11-llvm-18.1.8'))/'root/usr'
env=os.environ.copy();env.update(LLVM_CONFIG=str(llvm/'lib/llvm-18/bin/llvm-config'),LD_LIBRARY_PATH=str(llvm/'lib/x86_64-linux-gnu')+':'+str(llvm/'lib/llvm-18/lib'))
subprocess.run(['python3','tools/hash-steam-native-snapshot.py','verify'],cwd=root,check=True)
subprocess.run(['python3','tools/verify-steam-tested-inputs.py'],cwd=root,check=True)
ui=root/'build/steam-native-experiment'
names='runtime backend_updates artwork controller downloads ready_launch preparation launch_details ui_adapter remember_setting controller_layouts cloud_runtime cloud_preflight redist_fetch vapor_title_remover uninstall_journal uninstall_registry uninstall_quarantine protected_uninstall title_removal playden_title_removal_host uninstall_flow title_absence native_title_absence vapor_game_cleanup native_paths native_session native_trust native_startup native_bridge native_app_mount native_game_runtime playden_launch_host playden_preparation_host cloud_launch_gate native_host_config native_kit_admission title_list_writer title_list_merge dedicated_title_list native_game_startup native_title_provision app_user_identity vapor_privilege'.split()
jobs=[(root/'src/steam_integration'/f'{n}.cpp',out/f'{n}.o') for n in names]
jobs += [(ui/'src/ps5.cpp',out/'ps5.o')]
for n in ('artwork_backend','title_icon','artwork'):
    jobs.append((source/'src/homebrew'/f'{n}.cpp',out/f'homebrew_{n}.o'))
jobs.append((source/'src/platform/ps5/console_services.cpp',out/'console_services.o'))
# Embed the existing independently hash-qualified cleanup payload; its source
# generator refuses any change to the console-tested ELF.
payload=root/'build/vapor-game-cleaner-pinned.elf'
data=payload.read_bytes()
if hashlib.sha256(data).hexdigest()!='0de8cbe5441aba99859af79857c50a8fad897c0e4cfee5511b1654266aaa3ea2':
    raise SystemExit('Console-qualified cleanup helper is missing or changed')
blob=out/'cleanup_blob.cpp'
blob.write_text('#include <cstddef>\nextern "C" {\nextern const unsigned char vapor_game_cleaner[] = {'+','.join(str(x) for x in data)+'};\nextern const std::size_t vapor_game_cleaner_size=sizeof(vapor_game_cleaner);\n}\n')
jobs.append((blob,out/'cleanup_blob.o'))
flags=['-std=c++20','-O2','-ffunction-sections','-fdata-sections','-DPLAYDEN_PLATFORM_PS5=1','-DWINE11_DEDICATED_HOST=1','-DWINE11_EXPERIMENT_UNINSTALL=1','-DWINE11_PAYLOAD_GAME_CLEANUP=1','-DWINE11_SERVICE_QUIESCENCE=1','-DWINE11_DISABLE_UNSAFE_CANCEL=1']
includes=[source/'src',root/'src',root/'src/steam_integration',ui/'src',source/'third_party/stb',source/'third_party/qrcodegen',root/'build/steam-network-tested-snapshot/include',ui/'src/owner_admission/third_party/mbedtls-publication/include',ui/'src/owner_admission/third_party/mbedtls-publication']
subprocess.run(['python3','tools/prepare_vapor_file_shims.py',str(out/'file_shims.c')],cwd=root,check=True)
c_jobs=[(out/'file_shims.c',out/'file_shims.o'),(source/'third_party/qrcodegen/qrcodegen.c',out/'qrcodegen.o')]
for src,obj in c_jobs:
    subprocess.run([str(sdk/'bin/prospero-clang'),'-O2','-ffunction-sections','-fdata-sections','-c',str(src),'-o',str(obj)],env=env,check=True)
def compile(job):
    src,obj=job
    with obj.with_suffix('.log').open('w') as log:
        result=subprocess.run([str(sdk/'bin/prospero-clang++'),*flags,*['-I'+str(x) for x in includes],'-c',str(src),'-o',str(obj)],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
    return obj,result.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results=list(pool.map(compile,jobs))
failed=[obj for obj,status in results if status]
if failed:
    for obj in failed:print(obj.with_suffix('.log').read_text()[-3000:])
    raise SystemExit('Candidate compilation failed')
objects=[str(obj) for _,obj in jobs]+[str(obj) for _,obj in c_jobs]
objects += [str(x) for x in sorted((ui/'fresh-ui').glob('*.o')) if x.name!='ps5.o']
objects += [str(x) for x in sorted((ui/'fresh-owner').glob('*.o'))]
objects += [str(ui/(n+'.o')) for n in ('app_crt','runtime_shims','app_heap','allocation_probe')]
native=root/'vendor/ps5-native-app-boilerplate/tooling/native'
script=out/'ps5-pie.ld'
script.write_text((native/'ps5-pie.ld').read_text()+'\nPROVIDE(__eh_frame_start = ADDR(.eh_frame));\nPROVIDE(__eh_frame_end = ADDR(.eh_frame) + SIZEOF(.eh_frame));\nPROVIDE(__eh_frame_hdr_start = ADDR(.eh_frame_hdr));\nPROVIDE(__eh_frame_hdr_end = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr));\n')
archives=[str(x) for x in sorted(native_build.glob('lib*.a'))]
archives += [str(sdk/'target/lib'/n) for n in ('libunwind.a','libc++abi.a','libc++.a')]
archives += [str(root/'build/steam-network-tested-snapshot/lib'/n) for n in ('libcurl.a','libmbedtls.a','libmbedx509.a','libmbedcrypto.a','libeverest.a','libp256m.a')]
archives += [str(ui/'compiler-rt/libclang_rt.builtins-x86_64.a')]
cmd=[str(sdk/'bin/prospero-lld'),'-T',str(script),'--eh-frame-hdr','--gc-sections','-Map='+str(out/'app.map'),*['--wrap='+n for n in ('fstatat','unlinkat','malloc','calloc','realloc','free','posix_memalign','malloc_usable_size','openat','mkdirat')],'-L',str(sdk/'target/lib'),'--version-script',str(native/'app-symbols.map'),'-e','_start','-o',str(out/'app.elf'),*objects,'--start-group',*archives,'--end-group','--as-needed',*[str(x) for x in sorted((root/'build/native-imports').glob('*.so'))],*[str(x) for x in sorted((sdk/'target/lib').glob('*.so'))]]
with (out/'link.log').open('w') as log:
    subprocess.run(cmd,cwd=root,env=env,check=True,stdout=log,stderr=subprocess.STDOUT)
def digest_file(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        while chunk:=stream.read(1<<20):h.update(chunk)
    return h.hexdigest()
inputs={str(path.relative_to(root)):digest_file(path) for path,_ in jobs if path.is_relative_to(root)}
inputs.update({str(path.relative_to(root)):digest_file(path) for path in native_build.glob('lib*.a')})
(out/'candidate.json').write_text(json.dumps({'backend':'0.11.0','source':'7d30aaa7dc3011561eae728badc133a6bcc7f73b','sha256':digest_file(out/'app.elf'),'console_tested':False,'inputs':inputs},indent=2)+'\n')
print('PASS: complete native candidate linked:',out/'app.elf')
