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
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_cs2_runtime as cs2

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
VAPOR_RECIPES = ROOT / "assets/vapor-recipes"
VAPOR_RECIPE_MANIFEST_SHA256 = "558296c8c4126a208f535e8a81fd80465111caf3969b42170c8e3b24808d0c62"
RECIPE_LIST = "runtime/recipes/recipes.lst"

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

def vapor_recipe_overlay(original_list: bytes) -> dict[str, bytes]:
    """Reviewed defaults for normal preparation, with saved user values left intact.

    The backend selects shipped recipes during preparation and layers saved
    user settings after unlocked defaults. This staging step never rewrites
    existing bottles or settings. Newer unsupported recipe effects are omitted.
    """
    if VAPOR_RECIPES.is_symlink() or not VAPOR_RECIPES.is_dir():
        raise ValueError("Vapor recipe source is not an ordinary directory")
    manifest_path = VAPOR_RECIPES / "manifest.json"
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise ValueError("Vapor recipe manifest is not an ordinary file")
    raw = manifest_path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != VAPOR_RECIPE_MANIFEST_SHA256:
        raise ValueError("Reviewed Vapor recipe manifest differs")
    manifest = json.loads(raw)
    if manifest.get("schema") != "vapor-recipe-overlay/1" or not isinstance(manifest.get("recipes"), list):
        raise ValueError("Invalid Vapor recipe manifest")
    allowed = {"recipe_id", "recipe_version", "app_id", "name", "launch_entries", "arguments",
               "resolution_arguments", "dll_overrides", "architecture", "runtime", "prefix", "graphics",
               "defaults", "locked"}
    result = {}
    names, apps = set(), set()
    upstream_names = {line.strip() for line in original_list.decode("utf-8").splitlines()
                      if line.strip() and not line.lstrip().startswith("#")}
    for entry in manifest["recipes"]:
        name = entry.get("file", "")
        app = entry.get("app_id")
        if (not isinstance(name, str) or not name.endswith(".json") or "/" in name or "\\" in name or
                ".." in name or name in names or name in upstream_names or
                not isinstance(app, int) or isinstance(app, bool) or app <= 0 or app in apps):
            raise ValueError("Invalid or duplicate Vapor recipe identity")
        path = VAPOR_RECIPES / name
        if path.is_symlink() or not path.is_file():
            raise ValueError("Vapor recipe is not an ordinary file: " + name)
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry.get("sha256"):
            raise ValueError("Vapor recipe differs from its reviewed digest: " + name)
        recipe = json.loads(data)
        if (set(recipe) - allowed or recipe.get("app_id") != app or
                recipe.get("recipe_id") != name[:-5] or recipe.get("locked") != []):
            raise ValueError("Vapor recipe has unsupported effects or locked user values: " + name)
        names.add(name)
        apps.add(app)
        result["runtime/recipes/" + name] = data
    actual = {path.name for path in VAPOR_RECIPES.iterdir()}
    if actual != names | {"manifest.json", "LICENSE"}:
        raise ValueError("Vapor recipe source inventory differs")
    separator = b"" if not original_list or original_list.endswith(b"\n") else b"\n"
    result[RECIPE_LIST] = original_list + separator + b"# Reviewed Vapor defaults; provenance: assets/vapor-recipes/manifest.json\n" + (
        "\n".join(sorted(names)) + "\n").encode("utf-8")
    return result


def expected_runtime_asset_hashes() -> dict[str, str]:
    """Expected staged inventory, including the independently reviewed overlay."""
    pinned = dict(read_runtime_asset_lock())
    source_list = REPO / RECIPE_LIST
    if source_list.is_symlink() or not source_list.is_file():
        raise ValueError("Pinned recipe list is not an ordinary file")
    original = source_list.read_bytes()
    if hashlib.sha256(original).hexdigest() != pinned.get(RECIPE_LIST):
        raise ValueError("Pinned Playden recipe list differs")
    pinned.update({name: hashlib.sha256(data).hexdigest() for name, data in vapor_recipe_overlay(original).items()})
    cs2.check_header()
    pinned.update(cs2.expected_hashes())
    return pinned


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
    original_list = next(data for name, data, _ in items if name == RECIPE_LIST)
    overlay = vapor_recipe_overlay(original_list)
    items = [(name, data, digest) for name, data, digest in items if name not in overlay]
    items.extend((name, data, hashlib.sha256(data).hexdigest()) for name, data in sorted(overlay.items()))
    ca = ca_bundle if ca_bundle is not None else ROOT / "build/steam-network-tested-snapshot/cacert.pem"
    if ca.is_symlink() or not ca.is_file(): raise ValueError("Tested Steam CA bundle is missing")
    ca_data = ca.read_bytes()
    if len(ca_data) != 188900 or hashlib.sha256(ca_data).hexdigest() != CA_SHA256:
        raise ValueError("Steam CA bundle differs from the console-tested input")
    items.append(("assets/steam/cacert.pem", ca_data, CA_SHA256))
    cs2.check_header()
    for entry in cs2.prepare():
        name = cs2.PREFIX + entry['path']
        items.append((name, (cs2.DEFAULT / entry['path']).read_bytes(), entry['sha256']))
    for name, path in cs2.notice_files().items():
        data = path.read_bytes()
        items.append((name, data, hashlib.sha256(data).hexdigest()))
    destination.mkdir(mode=0o700)
    receipt = {"source_revision": REVISION, "vapor_recipe_overlay_sha256": VAPOR_RECIPE_MANIFEST_SHA256, "files": {}}
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
