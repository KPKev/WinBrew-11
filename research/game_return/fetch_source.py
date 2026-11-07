#!/usr/bin/env python3
"""Prepare the exact experimental launcher source in a new local checkout.
This does not build, distribute, install or qualify a game launcher.
"""
import hashlib
from pathlib import Path
import subprocess

REVISION = 'ce289425bb34acbcc98f77b4e00ac98b3c16e4bc'
MAIN_SHA256 = 'a8372d5097e84251ec762beeed3e0f521db596f3d6c2ea9d5c5863532b462def'
URL = 'https://github.com/leolobato/prospero-win.git'
ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / 'build/return-wrapper/source'

def verify(source: Path):
    source = source.resolve(strict=True)
    def git(*args):
        return subprocess.check_output(['git','-c','safe.directory='+str(source),'-C',str(source),*args],text=True,stderr=subprocess.PIPE).strip()
    if Path(git('rev-parse','--show-toplevel')).resolve() != source:
        raise ValueError('Launcher source must be its own Git repository')
    if git('rev-parse','HEAD') != REVISION or git('status','--porcelain','--untracked-files=normal'):
        raise ValueError('Launcher source differs from the exact clean revision')
    main = source / 'native/wine64_main.c'
    if main.is_symlink() or hashlib.sha256(main.read_bytes()).hexdigest() != MAIN_SHA256:
        raise ValueError('Launcher main source differs from the pinned revision')
    return source

def prepare(destination: Path = DESTINATION):
    if destination.exists() or destination.is_symlink(): return verify(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    # mkdir is exclusive. An interrupted checkout is deliberately preserved.
    destination.mkdir()
    subprocess.run(['git','-C',str(destination),'init'],check=True)
    # The pinned SHA-256 covers LF source bytes. Do not inherit Windows Git's
    # global autocrlf setting, and keep subsequent operations consistent.
    subprocess.run(['git','-C',str(destination),'config','core.autocrlf','false'],check=True)
    subprocess.run(['git','-C',str(destination),'config','core.eol','lf'],check=True)
    subprocess.run(['git','-C',str(destination),'fetch','--depth','1',URL,REVISION],check=True)
    subprocess.run(['git','-C',str(destination),'checkout','--detach','FETCH_HEAD'],check=True)
    return verify(destination)

if __name__ == '__main__':
    print('Verified local experimental launcher source:', prepare())
