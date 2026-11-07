"""Package corresponding local sources, including experimental launcher research."""
from pathlib import Path
import os
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = {'.git', '.deps', 'build', 'dist', 'wrappedwine', 'wine11', '__pycache__'}
NAMES = ['art','src','tools','tests','assets','sce_sys','licenses','memory','evidence',
         'research/game_return','README.md','LICENSE','THIRD_PARTY_NOTICES.md',
         'dependencies.json','Preview.cmd','.gitignore','.gitattributes']
VENDORS = ['ps5-native-app-boilerplate','playden-lapy','playden-source','ps5log','selawik']
REQUIRED_LAUNCHER = ['wrapper-return.patch','return_intent.c','return_intent.h',
                     'return_native.c','return_native.h','apply_variant.py','fetch_source.py','design.json']

def create_archive(root: Path, destination: Path):
    root = root.resolve()
    launcher = root / 'research/game_return'
    for name in REQUIRED_LAUNCHER:
        path = launcher / name
        if not path.is_file() or path.is_symlink():
            raise ValueError('Launcher research source is missing or is a link: ' + name)
    paths = [root / n for n in NAMES] + [root / 'vendor' / n for n in VENDORS]
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in paths:
            def files(item):
                if item.is_file(): yield item; return
                for folder, dirs, names in os.walk(item):
                    dirs[:] = [d for d in dirs if d not in EXCLUDED]
                    for name in names: yield Path(folder) / name
            for f in files(path):
                if f.is_symlink() or not f.is_file(): continue
                rel = f.relative_to(root)
                if any(p in EXCLUDED for p in rel.parts): continue
                z.write(f, 'WinBrew11/' + rel.as_posix())
    print('Corresponding source archive ready; launcher return research remains experimental.')

if __name__ == '__main__':
    create_archive(ROOT, ROOT / 'dist/WinBrew11-M1-source.zip')
