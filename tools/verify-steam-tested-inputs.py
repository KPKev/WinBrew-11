"""Check the experimental PS5 network inputs against the console-tested snapshot.

This is independent of Playden's upstream lock and does not certify a release.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, default=ROOT / "build/steam-network-tested-snapshot")
    args = parser.parse_args()
    prefix = args.prefix
    manifest = json.loads((ROOT / "tools/steam-network-tested-inputs.json").read_text())
    expected = manifest["files"]
    errors = []
    for name, digest in expected.items():
        path = prefix / name
        if path.is_symlink() or not path.is_file():
            errors.append(name + " (missing or not a regular file)")
        elif hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            errors.append(name + " (changed)")
    include = prefix / "include"
    actual_headers = {p.relative_to(prefix).as_posix() for p in include.rglob("*") if p.is_file() or p.is_symlink()}
    expected_headers = {name for name in expected if name.startswith("include/")}
    errors.extend(name + " (unexpected)" for name in sorted(actual_headers - expected_headers))
    if errors:
        parser.exit(1, "Experimental Steam input verification failed: " + ", ".join(errors) + "\n")
    print("PASS: console-probed experimental archives, headers and CA bundle match their recorded hashes")
    print("This check does not establish complete Steam functionality or release readiness.")


if __name__ == "__main__":
    main()
