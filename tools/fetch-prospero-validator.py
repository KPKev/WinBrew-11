#!/usr/bin/env python3
"""Fetch the exact Playden-pinned validator source without replacing files.
Partial checkouts are retained for inspection; no kit is built or installed.
"""
from pathlib import Path
import subprocess
from prospero_validator_source import DEFAULT, REVISION, URL, verify_source

def prepare(destination: Path = DEFAULT):
    if destination.exists() or destination.is_symlink():
        return verify_source(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'clone', '--no-checkout', URL, str(destination)], check=True)
    # A Windows global autocrlf setting otherwise changes pinned shell-source
    # bytes and can make them unusable under WSL. Limit this to the new clone.
    subprocess.run(['git', '-C', str(destination), 'config', 'core.autocrlf', 'false'], check=True)
    subprocess.run(['git', '-C', str(destination), 'config', 'core.eol', 'lf'], check=True)
    subprocess.run(['git', '-C', str(destination), 'checkout', '--detach', REVISION], check=True)
    return verify_source(destination)

if __name__ == '__main__':
    print('Verified pinned local validator source:', prepare())
