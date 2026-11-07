#!/usr/bin/env python3
"""Build a separately titled experimental Steam app; never install it."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "build/steam-review-candidate-4/PPSA77381"
PIE = ROOT / "build/steam-native-experiment/experimental-pie.elf"
HELPER = ROOT / "build/wine11-lapy-source/build/owned_root_helper-PPSA77382"
SDK = ROOT / "vendor/playden-source/deps/ps5-payload-sdk"
TOOL = ROOT / "vendor/ps5-native-app-boilerplate/build/host/ps5-native-tool"
HELPER_HASH = "2d14818a36906f988da7a9e2f629c08a666660019e343d24650c458d6514a152"

def copy_tree_contents(source, destination):
    destination.mkdir(parents=True, exist_ok=False)
    for item in source.rglob('*'):
        if item.is_symlink(): raise ValueError("Symbolic links are not allowed")
        target = destination / item.relative_to(source)
        if item.is_dir(): target.mkdir(parents=True, exist_ok=True)
        elif item.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item, target)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        while block := f.read(1024 * 1024): h.update(block)
    return h.hexdigest()

def main():
    if len(sys.argv) not in (2, 4): raise SystemExit("usage: assemble-steam-isolated-title.py NEW_OUTPUT_DIRECTORY [REVIEW_APP LINK_LOG]")
    output = Path(sys.argv[1]).resolve()
    if output.exists() or ROOT not in output.parents or not output.parent.is_dir(): raise ValueError("Output must be new and inside the workspace")
    source = Path(sys.argv[2]).resolve() if len(sys.argv) == 4 else SOURCE
    link_log = Path(sys.argv[3]).resolve() if len(sys.argv) == 4 else ROOT / "build/steam-preparation-fix-build.log"
    if ROOT not in source.parents or ROOT not in link_log.parents: raise ValueError("Inputs must be inside the workspace")
    subprocess.run([sys.executable, str(ROOT / "tools/hash-steam-native-snapshot.py"), "verify"], check=True)
    subprocess.run([sys.executable, str(ROOT / "tools/verify_steam_depot_snapshot.py")], check=True)
    with tempfile.TemporaryDirectory(prefix="wine11-quiescence-") as temp:
        subprocess.run([sys.executable, str(ROOT / "tools/prepare_steam_quiescence.py"), temp], check=True)
        for name in ("install_service.h", "install_service.cpp"):
            actual = ROOT / "build/steam-native-experiment/src/installs" / name
            expected = Path(temp) / "installs" / name
            if not actual.is_file() or actual.read_bytes() != expected.read_bytes():
                raise ValueError("Install service differs from the reviewed quiescence overlay")
    startup = (ROOT / "build/steam-native-experiment/src/ps5.cpp").read_text()
    if ("NativeGameStartup" not in startup or "MakeConsoleTitleProvision(data_dir)" not in startup or
            "data_dir >= 0 ? data_dir : -2" not in startup):
        raise ValueError("Snapshot lacks the dedicated main-drive game host; refusing a private-storage build")
    log = link_log.read_text()
    if "PASS: research PIE link only" not in log or "PASS: linked Playden real Steam runtime" not in log:
        raise ValueError("Updated native build did not complete")
    manifest = json.loads((source / "manifest.json").read_text())
    actual = {p.relative_to(source).as_posix() for p in source.rglob("*") if p.is_file() and p.name != "manifest.json"}
    if actual != set(manifest["files"]) or any(p.is_symlink() for p in source.rglob("*")):
        raise ValueError("Source package inventory differs")
    if any(digest(source / name) != value for name, value in manifest["files"].items()): raise ValueError("Source package differs")
    helper = json.loads((HELPER / "lapy-manifest.json").read_text())
    old_helper = json.loads((source / "lapy-manifest.json").read_text())
    if (helper.get("target_title") != "PPSA77382" or helper.get("elf_sha256") != HELPER_HASH or
        digest(HELPER / "lapy.elf") != HELPER_HASH or helper.get("protocol_sha256") != old_helper.get("protocol_sha256") or
        helper.get("console_validated") is not False): raise ValueError("Test helper identity differs")
    data = PIE.read_bytes()
    paths = ((b"/mnt/sandbox/PPSA77381_000/download0", b"/mnt/sandbox/PPSA77382_000/download0"),
             (b"/mnt/sandbox/PPSA77381_000/app0", b"/mnt/sandbox/PPSA77382_000/app0"))
    if data.count(b"PPSA77381") != 2 or any(data.count(old) != 1 or len(old) != len(new) for old, new in paths):
        raise ValueError("Unexpected native title references")
    patched = data
    for old, new in paths: patched = patched.replace(old, new)
    remover_path = b"/data/homebrew/PPSA77382/vapor-title-remover.elf"
    remover_refs = data.count(remover_path)
    existing_target_refs = data.count(b"PPSA77382")
    if os.environ.get("WINE11_EXPERIMENT_UNINSTALL") == "1":
        # The optimizer may also emit a prefix of the literal for comparisons.
        if remover_refs != 1 or existing_target_refs != data.count(b"/data/homebrew/PPSA77382/vapor-t"):
            raise ValueError("Unexpected Vapor remover references")
    elif existing_target_refs:
        raise ValueError("Unexpected Vapor remover path")
    if patched.count(b"PPSA77381") or patched.count(b"PPSA77382") != 2 + existing_target_refs:
        raise ValueError("Title transformation failed")
    output.mkdir(mode=0o700)
    app = output / "PPSA77382"; copy_tree_contents(source, app)
    shutil.copyfile(ROOT / "sce_sys/icon0.png", app / "sce_sys/icon0.png")
    logo = ROOT / "assets/steam-header-logo.png"
    if not logo.is_file() or digest(logo) != "6cb869df089146c12efb5e9c968e911c314842624ba6f052a11346ac734cadc8":
        raise ValueError("Steam header asset differs from the reviewed original")
    shutil.copyfile(logo, app / "assets/steam-header-logo.png")
    badge = ROOT / "assets/vapor-icon-v1.png"
    if not badge.is_file() or digest(badge) != "3fae9f2ab19e413bcae353b1ff34b4cf7cd0f1fca809e5faeab2339a85671fa6":
        raise ValueError("Approved Vapor icon differs from the reviewed image")
    shutil.copyfile(badge, app / "assets/vapor-icon-v1.png")
    shutil.copyfile(ROOT / "THIRD_PARTY_NOTICES.md", app / "THIRD_PARTY_NOTICES.md")
    (output / "test-pie.elf").write_bytes(patched)
    shutil.copyfile(HELPER / "lapy.elf", app / "lapy.elf")
    shutil.copyfile(HELPER / "lapy-manifest.json", app / "lapy-manifest.json")
    param_path = app / "sce_sys/param.json"; param = json.loads(param_path.read_text())
    if param["titleId"] != "PPSA77381" or "PPSA77381" not in param["contentId"]: raise ValueError("Unexpected source identity")
    param["titleId"] = "PPSA77382"; param["conceptId"] = "77382"
    param["contentId"] = param["contentId"].replace("PPSA77381", "PPSA77382")
    param["localizedParameters"]["en-US"]["titleName"] = "WinBrew 11"
    param_path.write_text(json.dumps(param, indent=2) + "\n")
    imports = ROOT / "build/native-imports"
    extra = [imports / name for name in ("libSceCommonDialog.so", "libSceMouse.so", "libSceWebBrowserDialog.so")]
    if any(not p.is_file() or p.is_symlink() for p in extra): raise ValueError("Missing native imports")
    elf = output / "test-eboot.elf"
    subprocess.run([str(TOOL), "link", "--in", str(output / "test-pie.elf"), "--out", str(elf), "--stub-dir", str(SDK / "target/lib"),
                    *[part for p in extra for part in ("--stub", str(p))], "--module-sdk", "0x02000009", "--companion-sdk", "0x08050001", "--file-name", "eboot.elf"], check=True)
    (app / "eboot.bin").unlink()
    subprocess.run([str(TOOL), "self", "--sign", "--in", str(elf), "--out", str(app / "eboot.bin"), "--magic", "0x1D3D154F"], check=True)
    manifest["console_tested"] = False; manifest["steam_experimental"] = True; manifest["isolated_test_title"] = "PPSA77382"
    manifest["project"] = "WinBrew 11"
    manifest["files"] = {p.relative_to(app).as_posix(): digest(p) for p in sorted(app.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    (app / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print("Prepared local isolated title:", app, "(not installed or console-tested)")

if __name__ == "__main__": main()
