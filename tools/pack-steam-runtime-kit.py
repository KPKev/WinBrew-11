#!/usr/bin/env python3
"""Build an offline transport ZIP of the verified, experimental Wine 11 kit.

This does not install anything. The caller must separately verify extraction,
ownership and permissions on the console before attempting to run a game.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import zipfile

ORIGINAL_ZIP_SHA256 = "6d58e72112c8e5d8ab0a4872f53cdbefb63adc436d366201cd02e704f74482b5"
DERIVED_MANIFEST_SHA256 = "1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc"
RELEASE_PREFIX = "playden-kit-0.13/"

def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as src:
        while block := src.read(1024 * 1024): h.update(block)
    return h.hexdigest()

def zipinfo(name, mode, directory=False):
    item = zipfile.ZipInfo(name + ("/" if directory else ""), (2020, 1, 1, 0, 0, 0))
    item.create_system = 3
    item.external_attr = ((stat.S_IFDIR if directory else stat.S_IFREG) | mode) << 16
    if directory: item.external_attr |= 0x10
    item.compress_type = zipfile.ZIP_STORED if directory else zipfile.ZIP_DEFLATED
    return item

def pack(source, upstream, output):
    source, upstream, output = Path(source).resolve(strict=True), Path(upstream).resolve(strict=True), Path(output).resolve()
    staging = output.with_name(output.name + ".building")
    if output.exists() or staging.exists() or not output.parent.is_dir() or source in output.parents:
        raise ValueError("Output and staging must be new files outside the source kit")
    if sha(upstream) != ORIGINAL_ZIP_SHA256: raise ValueError("Original release archive differs")
    manifest_path = source / "manifest.json"
    if sha(manifest_path) != DERIVED_MANIFEST_SHA256: raise ValueError("Derived kit manifest differs")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = {}
    for entry in manifest["files"]:
        name = entry["path"]; path = PurePosixPath(name)
        if (path.is_absolute() or str(path) != name or any(p in (".", "..") for p in path.parts)
            or "\\" in name or name in entries or entry["mode"] not in ("0644", "0755")):
            raise ValueError("Unsafe or repeated kit entry")
        entries[name] = entry
    actual = set()
    directories = []
    if stat.S_IMODE(source.stat().st_mode) != 0o755: raise ValueError("Kit root is not readable")
    for path in source.rglob("*"):
        relative = path.relative_to(source).as_posix()
        info = path.lstat()
        if stat.S_ISDIR(info.st_mode):
            if stat.S_IMODE(info.st_mode) != 0o755: raise ValueError("Unexpected directory mode: " + relative)
            directories.append(relative)
        elif stat.S_ISREG(info.st_mode): actual.add(relative)
        else: raise ValueError("Non-regular kit entry: " + relative)
    if actual != set(entries) | {"manifest.json"}: raise ValueError("Kit file inventory differs")
    with zipfile.ZipFile(upstream) as original, staging.open("xb") as file, zipfile.ZipFile(file, "w", allowZip64=True) as out:
        out.writestr(zipinfo("playden-kit", 0o755, True), b"")
        for name in sorted(directories): out.writestr(zipinfo("playden-kit/" + name, 0o755, True), b"")
        for name in sorted(actual):
            path = source / name
            expected = entries.get(name)
            mode = int(expected["mode"], 8) if expected else 0o644
            if stat.S_IMODE(path.stat().st_mode) != mode: raise ValueError("File mode differs: " + name)
            h = hashlib.sha256(); size = 0
            with path.open("rb") as src, out.open(zipinfo("playden-kit/" + name, mode), "w", force_zip64=True) as dest:
                while block := src.read(1024 * 1024): h.update(block); size += len(block); dest.write(block)
            expected_hash = expected["sha256"] if expected else DERIVED_MANIFEST_SHA256
            if h.hexdigest() != expected_hash or (expected and size != expected["size"]):
                raise ValueError("File content differs: " + name)
        notices = [i for i in original.infolist() if not i.is_dir() and
                   (i.filename.startswith(RELEASE_PREFIX + "LICENSES/") or i.filename in
                    (RELEASE_PREFIX + "SOURCES.md", RELEASE_PREFIX + "README.md"))]
        if not notices or len({i.filename for i in notices}) != len(notices): raise ValueError("Release notices missing or repeated")
        for item in sorted(notices, key=lambda i: i.filename):
            relative = item.filename[len(RELEASE_PREFIX):]
            if any(p in ("", ".", "..") for p in PurePosixPath(relative).parts) or "\\" in relative:
                raise ValueError("Unsafe notice path")
            out.writestr(zipinfo("release-notices/" + relative, 0o644), original.read(item))
        out.writestr(zipinfo("EXPERIMENTAL.txt", 0o644),
                     "Wine 11 experimental runtime kit. Not console-qualified. Preserve Unix file modes when extracting.\n")
    # No overwrite even if another writer creates the destination during packing.
    os.link(staging, output)
    staging.unlink()
    print("Prepared experimental transport:", output)
    print("SHA-256:", sha(output))
    print("Includes", len(entries), "kit files and", len(notices), "release notices; not installed or console-tested.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kit"); parser.add_argument("original_release_zip"); parser.add_argument("new_output_zip")
    args = parser.parse_args()
    pack(args.kit, args.original_release_zip, args.new_output_zip)
