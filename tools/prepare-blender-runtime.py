#!/usr/bin/env python3
from prepare_blender_runtime import *
if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser(description="Verify or unpack the pinned Blender compatibility bundle")
    p.add_argument('--archive', type=Path);p.add_argument('--destination',type=Path,default=DEFAULT);p.add_argument('--verify-only',action='store_true')
    a=p.parse_args();check_header();files=verify(a.destination) if a.verify_only else prepare(a.destination,a.archive)
    print(f"PASS: {len(files)} pinned Blender runtime/title files; no console changes")
