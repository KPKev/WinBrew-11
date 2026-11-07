#!/usr/bin/env python3
"""Fetch only the pinned public Steam CA bundle to a new local file.
This does not verify or qualify Steam networking libraries or deploy software.
"""
import argparse
import hashlib
from pathlib import Path
import urllib.request

URL = 'https://curl.se/ca/cacert-2026-09-25.pem'
EXPECTED_SHA256 = 'a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505'
EXPECTED_SIZE = 188900
ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'build/steam-inputs/cacert.pem'

def fetch(destination: Path, opener=urllib.request.urlopen):
    if not destination.parent.is_dir(): raise ValueError('Destination parent must already exist')
    # Check before requesting, and use exclusive creation after verification to
    # avoid replacing an existing user file even if another process races us.
    if destination.exists() or destination.is_symlink(): raise FileExistsError('Destination already exists')
    with opener(URL, timeout=30) as response:
        data = response.read(EXPECTED_SIZE + 1)
    if len(data) != EXPECTED_SIZE or hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise ValueError('Downloaded CA bundle does not match the pinned input')
    with destination.open('xb') as output: output.write(data)
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=DEFAULT)
    args = parser.parse_args()
    if args.output == DEFAULT: DEFAULT.parent.mkdir(parents=True, exist_ok=True)
    print('Verified pinned CA bundle:', fetch(args.output))
if __name__ == '__main__': main()
