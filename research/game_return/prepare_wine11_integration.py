"""Create an isolated, hash-checked Wine11 snapshot for observer integration.

Never edits the shared production source/build or any console file.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/owner-observer-integration/project'
FOLDERS = ['src', 'tools', 'tests', 'assets', 'sce_sys', 'licenses']
FILES = ['dependencies.json', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md',
         'Preview.cmd', '.gitignore', '.gitattributes']


def stock_files():
    result = []
    for folder in FOLDERS:
        result += [p for p in (ROOT / folder).rglob('*') if p.is_file()
                   and '__pycache__' not in p.parts and p.suffix != '.pyc']
    result += [ROOT / p for p in FILES if (ROOT / p).is_file()]
    return sorted(result)


def inventory():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in stock_files()}


if __name__ == '__main__':
    if OUT.exists():
        raise SystemExit('Isolated snapshot already exists; inspect it before creating another')
    if not OUT.resolve().is_relative_to((ROOT / 'build/owner-observer-integration').resolve()):
        raise SystemExit('Output path escaped its isolated directory')
    before = inventory()
    OUT.mkdir(parents=True)
    for relative in before:
        target = OUT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)
    if before != inventory():
        raise SystemExit('Shared input changed during snapshot; this copy is not admitted for testing')
    (OUT.parent / 'stock-source-sha256.json').write_text(json.dumps(before, indent=2) + '\n', newline='\n')
    subprocess.run(['git', '-C', str(OUT), 'init', '-q'], check=True)
    subprocess.run(['git', '-C', str(OUT), 'config', 'core.autocrlf', 'false'], check=True)
    subprocess.run(['git', '-C', str(OUT), 'add', '--', *FOLDERS, *[p for p in FILES if (OUT / p).exists()]], check=True)
    subprocess.run(['git', '-C', str(OUT), '-c', 'user.name=Wine 11 Build',
                    '-c', 'user.email=wine11-build@localhost', 'commit', '-qm', 'Snapshot current Wine11 for isolated owner observer integration'], check=True)
    print('Created stable isolated Wine11 snapshot:', OUT)
