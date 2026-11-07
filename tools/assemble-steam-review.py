#!/usr/bin/env python3
"""Assemble a local, explicitly experimental Wine 11 Steam review build.

This refuses stale inputs and never installs, uploads, or modifies the base
package. A successful result is not evidence of console or account behavior.
Run under WSL after the complete diagnostic link succeeds.
"""
from __future__ import annotations
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "dist/PPSA77381"
ASSETS = ROOT / "build/steam-app-assets-candidate"
BUILD = ROOT / "build/steam-native-experiment"
LOG = ROOT / "build/steam-native-remember.log"
SDK = ROOT / "vendor/playden-source/deps/ps5-payload-sdk"
TOOL = ROOT / "vendor/ps5-native-app-boilerplate/build/host/ps5-native-tool"

def copy_tree_contents(source, destination):
    destination.mkdir(parents=True, exist_ok=False)
    for item in source.rglob('*'):
        if item.is_symlink(): raise ValueError("Symbolic links are not allowed")
        target = destination / item.relative_to(source)
        if item.is_dir(): target.mkdir(parents=True, exist_ok=True)
        elif item.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item, target)


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(1024 * 1024): h.update(chunk)
    return h.hexdigest()

def verify_base() -> dict:
    manifest = json.loads((BASE / "manifest.json").read_text())
    if manifest.get("project") != "Wine 11" or not isinstance(manifest.get("files"), dict): raise ValueError("Unexpected base manifest")
    actual = {p.relative_to(BASE).as_posix() for p in BASE.rglob("*") if p.is_file() and p.name != "manifest.json"}
    if any(p.is_symlink() for p in BASE.rglob("*")) or actual != set(manifest["files"]): raise ValueError("Base package file set differs")
    for name, expected in manifest["files"].items():
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or digest(BASE / name) != expected: raise ValueError("Base package content differs: " + name)
    helper = json.loads((BASE / "lapy-manifest.json").read_text())
    if helper.get("target_title") != "PPSA77381" or helper.get("elf_sha256") != digest(BASE / "lapy.elf"):
        raise ValueError("Base helper does not match the Wine 11 title")
    return manifest

def verify_assets(module=None) -> dict:
    if module is None:
        spec = importlib.util.spec_from_file_location("runtime_assets", ROOT / "tools/stage-steam-runtime-assets.py")
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    receipt = json.loads((ASSETS / "runtime-assets.json").read_text())
    if receipt.get("source_revision") != module.REVISION or not isinstance(receipt.get("files"), dict): raise ValueError("Invalid asset receipt")
    actual = {p.relative_to(ASSETS).as_posix() for p in ASSETS.rglob("*") if p.is_file() and p.name != "runtime-assets.json"}
    if any(p.is_symlink() for p in ASSETS.rglob("*")) or actual != set(receipt["files"]): raise ValueError("Asset file set differs")
    pinned = dict(module.read_runtime_asset_lock())
    pinned["assets/steam/cacert.pem"] = module.CA_SHA256
    if actual != set(pinned): raise ValueError("Pinned asset set differs")
    for name in sorted(actual):
        data = (ASSETS / name).read_bytes()
        actual_sha = hashlib.sha256(data).hexdigest()
        if actual_sha != receipt["files"][name]: raise ValueError("Asset receipt mismatch: " + name)
        if actual_sha != pinned[name] or (name == "assets/steam/cacert.pem" and len(data) != 188900):
            raise ValueError("Asset differs from pinned Playden source: " + name)
    return receipt

def verify_current_steam(current_steam: Path, snap_steam: Path) -> None:
    current_files = {p.relative_to(current_steam).as_posix() for p in current_steam.rglob("*") if p.is_file()}
    snapshot_files = {p.relative_to(snap_steam).as_posix() for p in snap_steam.rglob("*") if p.is_file()} - {"guarded_cloud_service.cpp", "guarded_local_session_repository.cpp", "tuned_depot_installer.cpp", "observed_http_client.cpp", "space_aware_install_runner.cpp", "guarded_cm_app_details.cpp", "blender_depot_app_info.cpp"}
    if not current_files or current_files != snapshot_files or any((current_steam / name).read_bytes() != (snap_steam / name).read_bytes() for name in current_files):
        raise ValueError("Steam source changed since the diagnostic snapshot; rebuild it first")

def verify_generated_services(snap_steam: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="wine11-services-") as temp:
        subprocess.run([sys.executable, str(ROOT / "tools/prepare_steam_quiescence.py"), temp], check=True)
        for name in ("install_service.h", "install_service.cpp"):
            actual = snap_steam.parent / "installs" / name
            expected = Path(temp) / "installs" / name
            if not actual.is_file() or actual.read_bytes() != expected.read_bytes():
                raise ValueError("Install service differs from the reviewed quiescence overlay")
        expected = Path(temp) / "guarded_cm_app_details.cpp"
        subprocess.run([sys.executable, str(ROOT / "tools/prepare_steam_depot_estimates.py"),
                        str(ROOT / "vendor/playden-source/src/steam/backend/cm_app_details.cpp"), str(expected)], check=True)
        if not (snap_steam / expected.name).is_file() or (snap_steam / expected.name).read_bytes() != expected.read_bytes():
            raise ValueError("Depot estimates source differs from reviewed transformation")
        for script, name in (("prepare_blender_depot_selection.py", "blender_depot_app_info.cpp"),
                             ("prepare-steam-cloud-experiment.py", "guarded_cloud_service.cpp"),
                             ("prepare-steam-session-experiment.py", "guarded_local_session_repository.cpp"),
                             ("prepare-steam-depot-tuning.py", "tuned_depot_installer.cpp"),
                             ("prepare_steam_space_runner.py", "space_aware_install_runner.cpp"),
                             ("prepare_steam_http_observer.py", "observed_http_client.cpp")):
            expected = Path(temp) / name
            subprocess.run([sys.executable, str(ROOT / "tools" / script), str(expected)], check=True)
            if not (snap_steam / name).is_file() or (snap_steam / name).read_bytes() != expected.read_bytes():
                raise ValueError("Generated Steam service does not match its reviewed transformation: " + name)

def main() -> None:
    if len(sys.argv) not in (2, 3): raise SystemExit("usage: assemble-steam-review.py NEW_OUTPUT_DIRECTORY [COMBINED_LINK_LOG]")
    output = Path(sys.argv[1]).resolve()
    if output.exists() or not output.parent.is_dir() or ROOT not in output.parents:
        raise ValueError("Use a new output directory inside the workspace")
    link_log = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else LOG
    if ROOT not in link_log.parents or not link_log.is_file(): raise ValueError("The current diagnostic link has not completed")
    log = link_log.read_text()
    if "PASS: research PIE link only" not in log or "PASS: native snapshot matches recorded inputs" not in log:
        raise ValueError("The current complete diagnostic link has not passed")
    backend_log = link_log if len(sys.argv) == 3 else ROOT / "build/steam-backend-remember.log"
    if not backend_log.is_file() or "PASS: linked Playden real Steam runtime" not in backend_log.read_text():
        raise ValueError("The Steam backend diagnostic has not passed")
    subprocess.run([sys.executable, str(ROOT / "tools/hash-steam-native-snapshot.py"), "verify"], check=True)
    subprocess.run([sys.executable, str(ROOT / "tools/verify_steam_depot_snapshot.py")], check=True)
    verify_current_steam(ROOT / "src/steam_integration", BUILD / "src/steam_integration")
    verify_generated_services(BUILD / "src/steam_integration")
    source = (BUILD / "src/ps5.cpp").read_text()
    if "NativeGameStartup" not in source or "MakeConsoleTitleProvision" not in source:
        raise ValueError("Snapshot lacks the game-host entry point")
    base = verify_base(); baseline = json.dumps(base, sort_keys=True); assets = verify_assets()
    pie = BUILD / "experimental-pie.elf"
    snapshot_time = (BUILD / "source-hashes.json").stat().st_mtime_ns
    if (not pie.is_file() or pie.stat().st_mtime_ns < snapshot_time or pie.stat().st_mtime_ns > link_log.stat().st_mtime_ns or
            backend_log.stat().st_mtime_ns < snapshot_time):
        raise ValueError("PIE output is missing or newer than its link log")
    output.mkdir(mode=0o700)
    app = output / "PPSA77381"
    copy_tree_contents(BASE, app)
    for name in ('sce_sys/icon0.png', 'sce_sys/param.json', 'THIRD_PARTY_NOTICES.md'):
        shutil.copyfile(ROOT / name, app / name)
    for name in assets["files"]:
        dest = app / name; dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists(): raise ValueError("Steam asset would overwrite an existing package file: " + name)
        shutil.copyfile(ASSETS / name, dest)
    # The isolated remover has its own request namespace and is reproducibly
    # built from the pinned Playden source. Its exact bytes are rechecked by
    # the runtime before the loader receives them.
    if os.environ.get("WINE11_EXPERIMENT_UNINSTALL") == "1":
        subprocess.run(["bash", str(ROOT / "tools/build_vapor_title_remover.sh")], check=True)
        remover = ROOT / "build/vapor-title-remover.elf"
        if digest(remover) != "b9f421393039a3d2f3160d2b7e792883fb9aeff80d3540a9e589068500a1faf9":
            raise ValueError("Vapor title remover identity mismatch")
        if (app / "vapor-title-remover.elf").exists(): raise ValueError("Unexpected existing title remover")
        shutil.copyfile(remover, app / "vapor-title-remover.elf")
    converted = output / "experimental-eboot.elf"
    imports = ROOT / "build/native-imports"
    extra_stubs = [imports / name for name in ("libSceCommonDialog.so", "libSceMouse.so", "libSceWebBrowserDialog.so")]
    if any(not path.is_file() or path.is_symlink() for path in extra_stubs): raise ValueError("Native import descriptors are missing")
    subprocess.run([str(TOOL), "link", "--in", str(pie), "--out", str(converted), "--stub-dir", str(SDK / "target/lib"),
                    *[part for path in extra_stubs for part in ("--stub", str(path))],
                    "--module-sdk", "0x02000009", "--companion-sdk", "0x08050001", "--file-name", "eboot.elf"], check=True)
    (app / "eboot.bin").unlink()
    subprocess.run([str(TOOL), "self", "--sign", "--in", str(converted), "--out", str(app / "eboot.bin"),
                    "--magic", "0x1D3D154F"], check=True)
    (app / "STEAM-EXPERIMENTAL-NOTICE.txt").write_text(
        "Experimental review build; not console-qualified. Includes Playden runtime settings and gbe_fork Steam compatibility files. "
        "Their notices and licenses are in runtime/recipes, runtime/input and runtime/steamworks. "
        "This build does not include or install the separate Wine 11 game kit.\n")
    base["console_tested"] = False
    base["steam_experimental"] = True
    base["files"] = {p.relative_to(app).as_posix(): digest(p) for p in sorted(app.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    (app / "manifest.json").write_text(json.dumps(base, indent=2, sort_keys=True) + "\n")
    if json.dumps(verify_base(), sort_keys=True) != baseline: raise ValueError("Base package changed during assembly")
    print("Prepared local review build only:", app)
    print("Console execution, account, downloads, game kit, and game launch remain unverified.")

if __name__ == "__main__": main()
