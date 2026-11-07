#!/usr/bin/env python3
"""Prepare an isolated Wine 11 kit from a fully verified local Playden kit.

This is an offline preparation tool, not an installer or a console qualification.
The original kit, the upstream validator, and the installed console are untouched.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
from prospero_validator_source import verify_source

ROOT = Path(__file__).resolve().parents[1]
SOURCE_TOOLS = ROOT / "build/steam-host-deps/prospero-win"
HELPER_SHA256 = "76a1936fbf35b702379d479e6b856362887f0530f8a27da00c6d86bf748dc50f"
RULE = {"prefix": "FAKE78", "allow_list": "/data/wine11/title-rule/allow-list"}
ORIGINAL_RULE = {"prefix": "FAKE78", "allow_list": "/data/playden/title-rule/allow-list"}


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        while data := source.read(1024 * 1024):
            result.update(data)
    return result.hexdigest()


def write_json(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def derive(source: Path, output: Path, helper: Path, helper_manifest: Path,
           allow_development_build_paths: bool = False) -> None:
    sys.path.insert(0, str(verify_source(SOURCE_TOOLS)))
    from tools import package_playden_kit as kit

    source = source.resolve(strict=True)
    output = output.resolve()
    staging = output.with_name(output.name + ".building")
    if output.exists() or staging.exists() or source == output or source in output.parents:
        raise ValueError("Output and staging must be new paths outside the source kit")
    if not output.parent.is_dir():
        raise ValueError("Output parent does not exist")
    # The upstream validator checks every original file and its provenance.
    manifest = kit.validate_kit(source, development_build_paths=allow_development_build_paths)
    source_exceptions = manifest.pop("development_exceptions", [])
    variants = manifest.get("variants", [])
    if manifest.get("kit_id") != "playden-kit" or len(variants) != 1 or variants[0].get("any_title") is not True:
        raise ValueError("Expected the verified Playden any-title kit")
    variant = variants[0]
    old_helper = json.loads((source / variant["helper_manifest"]).read_text(encoding="utf-8"))
    old_build = json.loads((source / variant["native_build_manifest"]).read_text(encoding="utf-8"))
    replacement = json.loads(helper_manifest.read_text(encoding="utf-8"))
    if (old_helper.get("title_rule") != ORIGINAL_RULE or old_build.get("title_rule") != ORIGINAL_RULE or
            replacement.get("title_rule") != RULE or replacement.get("console_validated") is not False or
            replacement.get("elf_sha256") != HELPER_SHA256 or digest(helper) != HELPER_SHA256 or
            replacement.get("protocol_sha256") != digest(source / variant["helper_protocol"])):
        raise ValueError("Helper, protocol, or title-rule provenance does not match")

    # Copy only after validating the source. A failed attempt stays in staging
    # for inspection; it is never presented as a completed kit.
    shutil.copytree(source, staging)
    # The derived kit is read by the unprivileged app, often after being
    # copied by a root-owned installer. Preserve private source permissions
    # but make this newly created runtime root readable and traversable.
    staging.chmod(0o755)
    title = staging / variant["path"]
    shutil.copyfile(helper, title / "lapy.elf")
    write_json(staging / variant["helper_manifest"], replacement)
    old_build["title_rule"] = RULE
    write_json(staging / variant["native_build_manifest"], old_build)
    registry = staging / manifest["template"]["path"] / "user.reg"
    content = registry.read_bytes()
    old = kit._mono_runtime_path("playden-kit").replace("\\", "\\\\").encode()
    new = old.replace(b"data\\\\playden\\\\kits", b"data\\\\wine11\\\\kits")
    if old == new or content.count(old) != 1:
        raise ValueError("Original Wine Mono path is missing or ambiguous")
    registry.write_bytes(content.replace(old, new))
    entries = kit._file_entries(staging)
    manifest["files"] = entries
    manifest["files_sha256"] = kit._files_digest(entries)
    write_json(staging / "manifest.json", manifest)

    # Use the same validator with explicit local inputs for this derived kit;
    # this does not modify the upstream pin or claim console qualification.
    original_pin = kit.REQUIRED_HELPER_SHA256["FAKE78*"]
    original_root = kit.CONSOLE_KITS_ROOT
    try:
        kit.REQUIRED_HELPER_SHA256["FAKE78*"] = HELPER_SHA256
        kit.CONSOLE_KITS_ROOT = "/data/wine11/kits"
        checked = kit.validate_kit(staging, development_build_paths=allow_development_build_paths)
        if sorted(checked.get("development_exceptions", [])) != sorted(source_exceptions):
            raise ValueError("Derived kit changed the validator's build-path exceptions")
    finally:
        kit.REQUIRED_HELPER_SHA256["FAKE78*"] = original_pin
        kit.CONSOLE_KITS_ROOT = original_root
    staging.rename(output)
    print("Derived kit passed offline validation; helper is not console-qualified:", output)
    print("Manifest SHA-256:", digest(output / "manifest.json"))
    if source_exceptions:
        print(f"Local test only: {len(source_exceptions)} compiled files contain embedded build paths; not release-qualified.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Local copy of the complete original Playden kit")
    parser.add_argument("output", type=Path, help="New local output directory")
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--helper-manifest", type=Path, required=True)
    parser.add_argument("--allow-development-build-paths", action="store_true", help="Permit the validator's compiled-binary build-path exception for local testing")
    args = parser.parse_args()
    derive(args.source, args.output, args.helper, args.helper_manifest, args.allow_development_build_paths)


if __name__ == "__main__":
    main()
