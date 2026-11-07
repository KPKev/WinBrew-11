#!/usr/bin/env python3
"""Copy the observed Playden kit to a new local directory via read-only WFM.

This does not write to the console. It checks every file against the observed
manifest; the upstream full-kit validator is still required before use.
Run on a POSIX filesystem that preserves file modes, not a Windows mount.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_return_readiness import ReadOnlyWfm

CONSOLE_ROOT = "/data/playden/kits/playden-kit"
OBSERVED_MANIFEST = "d1dbc8e1b5b98691950369a89ef87c1dff5b7064a31844ccf20a4bed99b0e55b"
MAX_FILE = 64 * 1024 * 1024


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def entries(raw: bytes) -> list[dict]:
    if sha(raw) != OBSERVED_MANIFEST:
        raise ValueError("The console kit manifest changed; review it before copying")
    doc = json.loads(raw)
    if doc.get("kit_id") != "playden-kit" or not isinstance(doc.get("files"), list):
        raise ValueError("Unexpected kit manifest")
    seen = set()
    for item in doc["files"]:
        if not isinstance(item, dict) or set(item) != {"path", "size", "sha256", "mode"}:
            raise ValueError("Malformed file entry")
        name = item["path"]
        if not isinstance(name, str) or "\\" in name or "\x00" in name:
            raise ValueError("Invalid kit path")
        path = PurePosixPath(name)
        if path.is_absolute() or str(path) != name or any(part in (".", "..") for part in path.parts) or path.parts[0] in ("manifest.json", ".wine11-copy-state", ".wine11-copy-parts") or name in seen:
            raise ValueError("Unsafe or repeated kit path")
        if (not isinstance(item["size"], int) or isinstance(item["size"], bool) or
                not 0 <= item["size"] <= MAX_FILE or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) or
                item["mode"] not in ("0644", "0755")):
            raise ValueError("Unexpected file size, digest, or mode")
        seen.add(name)
    return doc["files"]


def copy(reader, destination: Path) -> None:
    raw = reader.read(CONSOLE_ROOT + "/manifest.json", 8 * 1024 * 1024)
    files = entries(raw)
    destination = destination.resolve()
    if not destination.parent.is_dir():
        raise ValueError("Destination parent is missing")
    marker = destination / ".wine11-copy-state"
    parts = destination / ".wine11-copy-parts"
    if not destination.exists():
        destination.mkdir(mode=0o700)
        with marker.open("xb") as output:
            output.write(OBSERVED_MANIFEST.encode("ascii"))
        marker.chmod(0o600)
        parts.mkdir(mode=0o700)
    else:
        # Resume only a private directory this copier previously initialized.
        if (destination.is_symlink() or marker.is_symlink() or parts.is_symlink() or
                not destination.is_dir() or not marker.is_file() or
                stat.S_IMODE(destination.stat().st_mode) != 0o700 or
                marker.read_bytes() != OBSERVED_MANIFEST.encode("ascii")):
            raise ValueError("Destination is not an unfinished copy of this kit")
        parts.mkdir(mode=0o700, exist_ok=True)
        if not parts.is_dir(): raise ValueError("Partial-copy directory is unavailable")
    for index, item in enumerate(files, 1):
        target = destination.joinpath(*PurePosixPath(item["path"]).parts)
        parent = destination
        for part in PurePosixPath(item["path"]).parts[:-1]:
            parent = parent / part
            if parent.is_symlink(): raise ValueError("A copy directory is a symbolic link")
            parent.mkdir(exist_ok=True, mode=0o700)
            if not parent.is_dir(): raise ValueError("A copy directory is unavailable")
        if target.is_symlink(): raise ValueError("A copied file is a symbolic link")
        if target.exists():
            if (not target.is_file() or target.stat().st_size != item["size"] or
                    sha(target.read_bytes()) != item["sha256"] or
                    stat.S_IMODE(target.stat().st_mode) != int(item["mode"], 8)):
                raise ValueError("An existing copied file differs from the manifest: " + item["path"])
            partial = parts / hashlib.sha256(item["path"].encode()).hexdigest()
            if partial.exists() or partial.is_symlink():
                if partial.is_symlink() or not partial.is_file() or not os.path.samefile(partial, target):
                    raise ValueError("Unexpected partial copy for an existing file")
                partial.unlink()  # finish an interrupted publication of our own hard link
            continue
        data = reader.read(CONSOLE_ROOT + "/" + item["path"], item["size"])
        if len(data) != item["size"] or sha(data) != item["sha256"]:
            raise ValueError("Console file did not match manifest: " + item["path"])
        partial = parts / hashlib.sha256(item["path"].encode()).hexdigest()
        if partial.is_symlink(): raise ValueError("A partial copy is a symbolic link")
        if partial.exists():
            if not partial.is_file(): raise ValueError("A partial copy is not a file")
            prefix = partial.read_bytes()
            if not data.startswith(prefix): raise ValueError("A partial copy differs from the console file")
            with partial.open("ab") as output: output.write(data[len(prefix):])
        else:
            with partial.open("xb") as output: output.write(data)
        partial.chmod(int(item["mode"], 8))
        os.link(partial, target)  # refuses to replace an existing file
        partial.unlink()
        if stat.S_IMODE(target.stat().st_mode) != int(item["mode"], 8):
            raise ValueError("Local filesystem did not preserve file permissions")
        if index % 100 == 0:
            print(f"Copied {index}/{len(files)} verified files", flush=True)
    manifest_path = destination / "manifest.json"
    if manifest_path.is_symlink(): raise ValueError("Local manifest is a symbolic link")
    if manifest_path.exists():
        if not manifest_path.is_file() or manifest_path.read_bytes() != raw:
            raise ValueError("Local manifest differs from the console manifest")
    else:
        with manifest_path.open("xb") as output: output.write(raw)
    manifest_path.chmod(0o644)
    parts.rmdir()  # fails rather than removing unexpected files
    marker.unlink()
    print("Copied kit files; validate the complete kit before deriving or using it.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=8889)
    args = parser.parse_args()
    copy(ReadOnlyWfm(args.host, args.port), args.destination)


if __name__ == "__main__":
    main()
