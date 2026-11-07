"""Fetch only Playden's pinned compiler runtime for an isolated PS5 link probe."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import urllib.request

root = Path(__file__).resolve().parents[1]
pin = json.loads((root / 'vendor/playden-source/dependencies.lock.json').read_text())['compiler_rt_builtins']
out = root / 'build/steam-native-experiment/compiler-rt'
out.mkdir(parents=True, exist_ok=True)
target = out / 'libclang_rt.builtins-x86_64.a'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
if target.is_file() and sha(target) == pin['sha256']:
    print('PASS: compiler runtime matches Playden pin')
    raise SystemExit(0)
with tempfile.TemporaryDirectory(prefix='wine11-compiler-rt-') as tmp:
    tmp = Path(tmp)
    archive = tmp / 'compiler-rt.deb'
    url = pin['url'].replace('http://', 'https://', 1)
    with urllib.request.urlopen(url, timeout=60) as src, archive.open('wb') as dst:
        while chunk := src.read(1024 * 1024):
            dst.write(chunk)
    if sha(archive) != pin['archive_sha256']:
        raise SystemExit('Compiler runtime archive hash mismatch')
    subprocess.run(['dpkg-deb', '-x', str(archive), str(tmp / 'unpacked')], check=True)
    member = tmp / 'unpacked' / pin['member']
    if sha(member) != pin['sha256']:
        raise SystemExit('Compiler runtime library hash mismatch')
    target.write_bytes(member.read_bytes())
print('PASS: compiler runtime archive and library match Playden pins')
