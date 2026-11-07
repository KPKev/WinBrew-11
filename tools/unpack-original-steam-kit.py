#!/usr/bin/env python3
"""Verify and unpack the original Playden 0.13 release kit locally.

Nothing is installed on the console. The ZIP and its kit manifest must match
the published release digest and the previously observed console manifest.
Run on a POSIX filesystem which preserves executable and private file modes.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys
import zipfile
import argparse
from prospero_validator_source import verify_source

ROOT = Path(__file__).resolve().parents[1]
ZIP_SHA = "6d58e72112c8e5d8ab0a4872f53cdbefb63adc436d366201cd02e704f74482b5"
MANIFEST_SHA = "d1dbc8e1b5b98691950369a89ef87c1dff5b7064a31844ccf20a4bed99b0e55b"
PREFIX = "playden-kit-0.13/playden-kit/"

def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while data := f.read(1024 * 1024): h.update(data)
    return h.hexdigest()

def safe_relative(name: str) -> bool:
    p = PurePosixPath(name)
    return bool(name) and "\\" not in name and "\x00" not in name and not p.is_absolute() and str(p) == name and all(x not in (".", "..") for x in p.parts)

def unpack(archive: Path, output: Path, allow_development_build_paths: bool = False) -> None:
    archive = archive.resolve(strict=True); output = output.expanduser().resolve()
    staging = output.with_name(output.name + ".extracting")
    if output.exists() or not output.parent.is_dir(): raise ValueError("Output must be a new directory")
    if staging.is_symlink() or (staging.exists() and not staging.is_dir()): raise ValueError("Invalid staging directory")
    if digest(archive) != ZIP_SHA: raise ValueError("Release ZIP does not match its published SHA-256")
    sys.path.insert(0, str(verify_source()))
    from tools.package_playden_kit import validate_kit
    with zipfile.ZipFile(archive) as z:
        infos = z.infolist(); by_name = {}
        for info in infos:
            name = info.filename.rstrip("/")
            if not safe_relative(name) or info.filename in by_name: raise ValueError("Unsafe or duplicate ZIP entry")
            mode = info.external_attr >> 16
            kind = stat.S_IFMT(mode)
            if kind not in (0, stat.S_IFREG, stat.S_IFDIR) or info.flag_bits & 1: raise ValueError("Unsupported ZIP entry")
            by_name[info.filename] = info
        manifest_name = PREFIX + "manifest.json"
        if manifest_name not in by_name: raise ValueError("Release ZIP does not contain the expected kit")
        raw = z.read(by_name[manifest_name])
        if len(raw) > 8 * 1024 * 1024 or hashlib.sha256(raw).hexdigest() != MANIFEST_SHA:
            raise ValueError("Kit manifest differs from the previously observed console kit")
        manifest = json.loads(raw)
        if manifest.get("kit_id") != "playden-kit" or not isinstance(manifest.get("files"), list): raise ValueError("Unexpected kit manifest")
        expected = {}
        for item in manifest["files"]:
            if not isinstance(item, dict) or set(item) != {"path", "size", "sha256", "mode"}: raise ValueError("Malformed kit file entry")
            name = item["path"]
            if not isinstance(name, str) or not safe_relative(name) or name == "manifest.json" or name in expected:
                raise ValueError("Unsafe or repeated kit path")
            if not isinstance(item["size"], int) or isinstance(item["size"], bool) or not 0 <= item["size"] <= 512 * 1024 * 1024:
                raise ValueError("Unexpected kit file size")
            if item["mode"] not in ("0644", "0755") or not isinstance(item["sha256"], str) or len(item["sha256"]) != 64:
                raise ValueError("Unexpected kit permissions or digest")
            expected[name] = item
        if not expected or len(expected) > 20000 or sum(v["size"] for v in expected.values()) > 3 * 1024**3:
            raise ValueError("Unexpected kit inventory size")
        kit_files = {n[len(PREFIX):] for n, info in by_name.items() if n.startswith(PREFIX) and not info.is_dir()}
        if kit_files != set(expected) | {"manifest.json"}: raise ValueError("ZIP kit files differ from manifest")
        if staging.exists():
            # A previous interrupted extraction is admitted only if its full
            # file set, modes, and contents already match this pinned kit.
            if any(p.is_symlink() for p in staging.rglob("*")): raise ValueError("Staging contains a symbolic link")
            existing = {p.relative_to(staging).as_posix() for p in staging.rglob("*") if p.is_file()}
            if existing != set(expected) | {"manifest.json"} or (staging / "manifest.json").read_bytes() != raw:
                raise ValueError("Existing staging does not match the complete kit inventory")
            for name, item in expected.items():
                path = staging / name
                if path.stat().st_size != item["size"] or stat.S_IMODE(path.stat().st_mode) != int(item["mode"], 8) or digest(path) != item["sha256"]:
                    raise ValueError("Existing staging file differs: " + name)
            resumed = True
        else:
            staging.mkdir(mode=0o700); resumed = False
        # Empty Windows profile directories are not in the file manifest but
        # are required by Wine and by the upstream kit validator.
        for name, info in by_name.items():
            if name.startswith(PREFIX) and info.is_dir():
                relative = name[len(PREFIX):].rstrip("/")
                if relative:
                    if not safe_relative(relative): raise ValueError("Unsafe kit directory")
                    (staging / relative).mkdir(parents=True, exist_ok=True)
        for index, (name, item) in enumerate(sorted(expected.items()), 1):
            if resumed: continue
            info = by_name[PREFIX + name]
            if info.file_size != item["size"]: raise ValueError("ZIP file size differs from manifest: " + name)
            path = staging.joinpath(*PurePosixPath(name).parts); path.parent.mkdir(parents=True, exist_ok=True)
            h = hashlib.sha256(); count = 0
            with z.open(info) as source, path.open("xb") as dest:
                while chunk := source.read(1024 * 1024):
                    count += len(chunk)
                    if count > item["size"]: raise ValueError("File exceeds its declared size")
                    h.update(chunk); dest.write(chunk)
            if count != item["size"] or h.hexdigest() != item["sha256"]: raise ValueError("Kit file hash mismatch: " + name)
            path.chmod(int(item["mode"], 8))
            if stat.S_IMODE(path.stat().st_mode) != int(item["mode"], 8): raise ValueError("Filesystem did not preserve permissions")
            if index % 500 == 0: print(f"Verified {index}/{len(expected)} kit files", flush=True)
        if not resumed:
            (staging / "manifest.json").write_bytes(raw)
            (staging / "manifest.json").chmod(0o644)
    checked = validate_kit(staging, development_build_paths=allow_development_build_paths)
    exceptions = checked.get("development_exceptions", [])
    if exceptions: print(f"Local test only: {len(exceptions)} compiled files contain embedded build paths; not release-qualified.")
    os.rename(staging, output)
    print("Original kit passed full offline validation:", output)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--allow-development-build-paths", action="store_true", help="Permit the validator's compiled-binary build-path exception for local testing")
    args = parser.parse_args()
    unpack(args.archive, args.output, args.allow_development_build_paths)
