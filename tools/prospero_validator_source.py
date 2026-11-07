"""Verify the pinned local prospero-win validator source before importing it."""
from pathlib import Path
import subprocess

REVISION = 'e709a0b72361c6c8e16c80ff93a3705a5e5421b7'
URL = 'https://github.com/leolobato/prospero-win.git'
ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'build/steam-host-deps/prospero-win'

def verify_source(source: Path = DEFAULT) -> Path:
    try:
        source = source.resolve(strict=True)
        def git(*args):
            return subprocess.check_output(['git', '-c', 'safe.directory=' + str(source), '-C', str(source), *args], text=True, stderr=subprocess.PIPE).strip()
        if Path(git('rev-parse', '--show-toplevel')).resolve() != source:
            raise ValueError('validator source must be a repository root')
        if git('rev-parse', 'HEAD') != REVISION:
            raise ValueError('validator source revision differs from the pinned Playden source')
        if git('status', '--porcelain', '--untracked-files=normal'):
            raise ValueError('validator source has local changes')
        if not (source / 'tools/package_playden_kit.py').is_file():
            raise ValueError('pinned kit validator is missing')
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError('Pinned validator source unavailable; run python tools/fetch-prospero-validator.py') from error
    return source
