#!/usr/bin/env python3
"""Stage pinned Playden runtime assets for an isolated Wine 11 build.

This only prepares a local directory; it does not package or deploy an app.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT / "vendor/playden-source"
REVISION = "9f5a7cbebeaf07fa3ea5324dce5ad8f374ca0aec"
STEAMWORKS = {
    "steam_api.dll": "cae4372bceda997d6c73cebc1c14eb27d39b74a7999d8f28384bdfd8d746a79c",
    "steam_api64.dll": "70d9826a402d3a78b0c07e9f7ea7c6832468a1113013f31ea570fccaa5445ea9",
    "steam.dll": "f813075716892ab19ee9b0c06000c328bd5f15e713692384a30ddbcfc9065e57",
}
ASSET_LOCK = ROOT / "tools/playden-runtime-assets-lock.json"
ASSET_LOCK_SHA256 = "96d3abab04da354a3b9984cfb540a2b72265533cb3d15d0455eac580352a1ecd"
CA_SHA256 = "a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505"

def git(*args: str) -> bytes:
    return subprocess.run(["git", "-C", str(REPO), *args], check=True, stdout=subprocess.PIPE).stdout

def read_runtime_asset_lock() -> dict[str, str]:
    lock_bytes = ASSET_LOCK.read_bytes()
    if hashlib.sha256(lock_bytes).hexdigest() != ASSET_LOCK_SHA256:
        raise ValueError("Pinned runtime asset lock differs")
    lock = json.loads(lock_bytes)
    if lock.get("source_revision") != REVISION or not isinstance(lock.get("files"), dict):
        raise ValueError("Pinned runtime asset lock has an unexpected identity")
    return lock["files"]

def stage(destination: Path, ca_bundle: Path | None = None) -> None:
    destination = destination.resolve()
    if destination.exists() or not destination.parent.is_dir():
        raise ValueError("Destination must be a new directory with an existing parent")
    if (REPO / ".git").exists() and git("rev-parse", "HEAD").decode().strip() != REVISION:
        raise ValueError("Playden checkout is not at the pinned revision")
    pinned = read_runtime_asset_lock()
    required = {"runtime/presets.json", "runtime/recipes/recipes.lst", "runtime/recipes/LICENSE",
                "runtime/input/LICENSE", "runtime/steamworks/LICENSE", "runtime/steamworks/README.md"}
    required.update("runtime/steamworks/" + name for name in STEAMWORKS)
    if not required.issubset(pinned): raise ValueError("Pinned runtime asset set is incomplete")
    items = []
    for name, expected in sorted(pinned.items()):
        rel = Path(name)
        if (rel.is_absolute() or ".." in rel.parts or "\\" in name or
            not (name == "runtime/presets.json" or name.startswith(("runtime/recipes/", "runtime/input/", "runtime/steamworks/")))):
            raise ValueError("Unsafe runtime asset path")
        local = REPO / rel
        if any((REPO.joinpath(*rel.parts[:i])).is_symlink() for i in range(1, len(rel.parts) + 1)) or not local.is_file():
            raise ValueError("Runtime asset is not an ordinary file: " + name)
        data = local.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected: raise ValueError("Runtime asset differs from the pinned source: " + name)
        if local.parent.name == "steamworks" and local.name in STEAMWORKS and digest != STEAMWORKS[local.name]:
            raise ValueError("Steam compatibility file differs from its upstream pin")
        items.append((name, data, digest))
    ca = ca_bundle if ca_bundle is not None else ROOT / "build/steam-network-tested-snapshot/cacert.pem"
    if ca.is_symlink() or not ca.is_file(): raise ValueError("Tested Steam CA bundle is missing")
    ca_data = ca.read_bytes()
    if len(ca_data) != 188900 or hashlib.sha256(ca_data).hexdigest() != CA_SHA256:
        raise ValueError("Steam CA bundle differs from the console-tested input")
    items.append(("assets/steam/cacert.pem", ca_data, CA_SHA256))
    destination.mkdir(mode=0o700)
    receipt = {"source_revision": REVISION, "files": {}}
    for name, data, digest in items:
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        receipt["files"][name] = digest
    (destination / "runtime-assets.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print(f"Staged {len(items)} pinned runtime assets; no console package was created.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--ca-bundle", type=Path, help="independently acquired copy of the same pinned CA bundle")
    args = parser.parse_args()
    stage(args.destination, args.ca_bundle)
