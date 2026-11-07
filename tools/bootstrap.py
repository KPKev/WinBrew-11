"""Fetch pinned open-source build inputs without updating existing checkouts."""
from pathlib import Path
import hashlib,io,json,subprocess,urllib.request,zipfile
r=Path(__file__).resolve().parents[1];lock=json.loads((r/'dependencies.json').read_text())
for name in ['ps5-native-app-boilerplate','playden-lapy','selawik','playden-reference']:
    dep=lock[name];p=r/dep['path']
    if not p.exists():
        subprocess.run(['git','-c','core.autocrlf=false','clone','--no-checkout',dep['url'],str(p)],check=True)
        subprocess.run(['git','-C',str(p),'config','core.autocrlf','false'],check=True)
        subprocess.run(['git','-C',str(p),'checkout','--detach',dep['revision']],check=True)
    if (p/'.git').exists():
        actual=subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip()
        if actual!=dep['revision']:raise RuntimeError(f'{name}: existing checkout differs; inspect manually')
        subprocess.run(['git','-C',str(p),'config','core.autocrlf','false'],check=True)
        if name == 'playden-reference' and subprocess.check_output(['git','-C',str(p),'status','--porcelain','--untracked-files=no'],text=True).strip():
            raise RuntimeError('playden-reference: tracked source has local changes; inspect manually')
fontdir=r/'vendor/selawik/release'
for name,expected in lock['playden-lapy']['source_sha256'].items():
    p=r/'vendor/playden-lapy'/name
    actual=p.read_bytes().replace(b'\r\n',b'\n')
    if hashlib.sha256(actual).hexdigest()!=expected:raise RuntimeError('Pinned helper source differs: '+name)
    if p.read_bytes()!=actual:p.write_bytes(actual)
if not fontdir.exists():
    with urllib.request.urlopen(lock['selawik']['font_archive'],timeout=60) as response:
        archive=zipfile.ZipFile(io.BytesIO(response.read()))
    fontdir.mkdir()
    for name in ['selawk.ttf','selawksb.ttf']: (fontdir/name).write_bytes(archive.read(name))
for name,expected in {'selawk.ttf':'e9d98518d8ac2817782a9a382430463a2e0793ea68350b695bb727d9a830ee1c','selawksb.ttf':'0a9e9d0549a10f24bef9b3a29e06fe6e0b5c21e7a784c503b048a307841a7783'}.items():
    if hashlib.sha256((fontdir/name).read_bytes()).hexdigest()!=expected:raise RuntimeError('Font digest mismatch: '+name)
p=r/'vendor/ps5log/ps5log.h'
if not p.exists():
    p.parent.mkdir(parents=True,exist_ok=True)
    with urllib.request.urlopen('https://raw.githubusercontent.com/leolobato/prospero-win/e709a0b72361c6c8e16c80ff93a3705a5e5421b7/native/ps5log/ps5log.h',timeout=60) as response:p.write_bytes(response.read())
if hashlib.sha256(p.read_bytes()).hexdigest()!='05373177e2c1cd348848014139646e8b35306da673ff5c0a9089c1247eda394f':raise RuntimeError('ps5log digest mismatch')
print('Pinned build inputs ready')
