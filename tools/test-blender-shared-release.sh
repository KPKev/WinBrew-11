#!/usr/bin/env bash
# Host-only checks; never connects to or modifies a console.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 tests/test_blender_runtime.py
python3 tests/blender_recycle_compat_test.py
gcc -O2 -Iruntime-patches/blender/launcher tests/blender_launcher_test.c runtime-patches/blender/launcher/pw_blender_compat.c -o "$work/launcher"
"$work/launcher"
g++ -std=c++20 -O2 tests/blender_shared_upgrade_test.cpp -o "$work/upgrade"
"$work/upgrade"
g++ -std=c++20 -O2 src/explorer_fileops.cpp src/privilege.cpp tests/blender_recycle_interop_test.cpp -o "$work/interop"
python3 - "$work/interop" <<'PY'
import importlib.util,tempfile,subprocess,sys
from pathlib import Path
s=importlib.util.spec_from_file_location('compat','runtime-patches/blender/launcher/winbrew_blender_startup.py');c=importlib.util.module_from_spec(s);s.loader.exec_module(c)
with tempfile.TemporaryDirectory() as t:
 root=Path(t);docs=root/'Documents';docs.mkdir();c.ROOTS=((str(docs),str(docs)),);p=docs/'scene.blend';p.write_bytes(b'BLENDER-safe-content');moved=Path(c.recycle(c.capture(str(p))));subprocess.run([sys.argv[1],str(moved.parent),str(p),str(root)],check=True);assert p.read_bytes()==b'BLENDER-safe-content'
print('PASS: Blender recycle format restored by actual WinBrew code')
PY
