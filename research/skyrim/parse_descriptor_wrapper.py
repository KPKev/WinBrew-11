#!/usr/bin/env python3
"""Parse stdin using the preserved native wrapper; never store input bytes."""
import hashlib, json, subprocess, sys, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base=root/'build/return-wrapper/source'
provider=root/'src/owner_admission/third_party/mbedtls-publication'
data=sys.stdin.buffer.read(8193)
if len(data)>8192:
    print(json.dumps({'verdict':'oversize','expected_title':'FAKE78005','length_at_least':8193}))
    sys.exit(1)
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'descriptor-wrapper'
    command=['cc','-std=c11','-D_DEFAULT_SOURCE','-O2','-ffunction-sections','-fdata-sections',
             '-I'+str(provider/'include'),'-I'+str(provider),'-DMBEDTLS_CONFIG_FILE="pw_publication_config.h"',
             str(root/'research/skyrim/descriptor_wrapper_harness.c'),
             str(base/'native/pw_title_publication_native.c'),str(base/'src/pw_title_launch.c'),
             '-Wl,--gc-sections','-o',str(exe)]
    build=subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    if build.returncode:
        print(json.dumps({'verdict':'harness-unavailable','expected_title':'FAKE78005'}));sys.exit(2)
    parsed=subprocess.run([str(exe)],input=data,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=5)
    verdict=parsed.stdout.decode('ascii','replace').strip()
    if verdict not in ('accepted','rejected') or parsed.returncode not in (0,1):
        print(json.dumps({'verdict':'harness-error','expected_title':'FAKE78005'}));sys.exit(2)
    print(json.dumps({'verdict':verdict,'expected_title':'FAKE78005','length':len(data),'sha256':hashlib.sha256(data).hexdigest()}))
    sys.exit(parsed.returncode)
