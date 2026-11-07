"""Record or verify all files in the isolated experimental native source snapshot."""
import hashlib
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
snapshot = root / 'build/steam-native-experiment/src'
receipt = root / 'build/steam-native-experiment/source-hashes.json'
if sys.argv[1:] not in (['record'], ['verify']):
    raise SystemExit('usage: hash-steam-native-snapshot.py record|verify')
if not snapshot.is_dir():
    raise SystemExit('Native source snapshot is missing')
files = sorted(p for p in snapshot.rglob('*') if p.is_file())
if not files or any(p.is_symlink() for p in files):
    raise SystemExit('Native snapshot is empty or contains a symbolic link')
hashes = {p.relative_to(snapshot).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
if sys.argv[1] == 'record':
    receipt.write_text(json.dumps(hashes, sort_keys=True, indent=2) + '\n')
else:
    if not receipt.is_file() or hashes != json.loads(receipt.read_text()):
        raise SystemExit('Native source snapshot changed during the build')
print('PASS: native snapshot ' + ('recorded' if sys.argv[1] == 'record' else 'matches recorded inputs'))
