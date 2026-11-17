#!/usr/bin/env python3
"""Build and embed the Recycle Bin helper used by the PS5 native app."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit('usage: build-recycle-helper.py OUTPUT_CPP')
sdk = Path(os.environ.get('PS5_PAYLOAD_SDK', root / 'vendor/playden-source/deps/ps5-payload-sdk'))
build = root / 'build/recycle-helper'
build.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
# Native builds supply the pinned LLVM explicitly; preserve that toolchain.
if not env.get('LLVM_CONFIG'):
    candidate = Path.home()/'.cache/wine11-llvm-18.1.8/root/usr'
    env['LLVM_CONFIG'] = str(candidate/'lib/llvm-18/bin/llvm-config')
    env['LD_LIBRARY_PATH'] = str(candidate/'lib/x86_64-linux-gnu')+':'+str(candidate/'lib/llvm-18/lib')
subprocess.run([sys.executable, 'tools/prepare_vapor_file_shims.py', str(build/'file-shims.c')], cwd=root, env=env, check=True)
subprocess.run([str(sdk/'bin/prospero-clang'), '-c', str(build/'file-shims.c'), '-o', str(build/'file-shims.o')], cwd=root, env=env, check=True)
elf = build/'recycle-helper.elf'
subprocess.run([str(sdk/'bin/prospero-clang++'), '-std=c++20', '-O2', '-DRECYCLE_HELPER_PROCESS=1', '-Isrc', 'src/recycle_helper_main.cpp', 'src/explorer_fileops.cpp', 'src/privilege.cpp', str(build/'file-shims.o'), '-Wl,--wrap=openat', '-Wl,--wrap=unlinkat', '-o', str(elf)], cwd=root, env=env, check=True)
data=elf.read_bytes()
marker=b'WB_RECYCLE_HELPER_REQUEST_SLOT_1'
if not data.startswith(b'\x7fELF') or data.count(marker)!=1:
    raise SystemExit('Invalid Recycle Bin helper')
at=data.index(marker)
if data[at+len(marker):at+4096]!=bytes(4096-len(marker)) or b'/data/winbrew-recycle-helper-fixture' in data:
    raise SystemExit('Invalid or test-only helper request slot')
sha=hashlib.sha256(data).hexdigest()
lines=[','.join(str(x) for x in data[i:i+24])+',' for i in range(0,len(data),24)]
source='\n'.join(['#include <cstddef>', 'extern "C" {', 'extern const unsigned char winbrew_recycle_helper[] = {', *lines, '};', 'extern const std::size_t winbrew_recycle_helper_size = sizeof(winbrew_recycle_helper);', 'extern const char winbrew_recycle_helper_sha256[] = "'+sha+'";', '}'])+'\n'
Path(sys.argv[1]).write_text(source,encoding='utf-8',newline='\n')
print('Built confined Recycle Bin helper:',sha)
