#!/usr/bin/env python3
"""Refresh the verified diagnostic after the saved-session UID guard change.

Only the single reviewed runtime.cpp substitution is admitted. This relinks
the backend and the complete native diagnostic; it never packages or deploys.
Run under WSL after the previous diagnostic is finished.
"""
from __future__ import annotations
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SNAP = ROOT / "build/steam-native-experiment/src"
BACKEND = ROOT / "build/steam-snapshot-native-link-probe"
SDK = ROOT / "vendor/playden-source/deps/ps5-payload-sdk"
NETWORK = ROOT / "build/steam-network-tested-snapshot"
OLD = b'remember(persist, [this](bool value) { return auth_sessions.SetPersist(value).ok(); }),'
NEW = b'remember(persist, [this](bool value) {\n          const auto uid = playden::ps5::HoldEffectiveUid();\n          return auth_sessions.SetPersist(value).ok();\n        }),'

def run(args, **kw):
    subprocess.run(args, cwd=ROOT, check=True, **kw)

def main() -> None:
    previous = ROOT / "build/steam-native-remember.log"
    if not previous.is_file() or "PASS: research PIE link only" not in previous.read_text():
        raise ValueError("The prior complete diagnostic has not passed")
    run([sys.executable, "tools/hash-steam-native-snapshot.py", "verify"])
    run([sys.executable, "tools/verify_steam_depot_snapshot.py"])
    current = ROOT / "src/steam_integration"
    old_root = SNAP / "steam_integration"
    current_files = {p.relative_to(current).as_posix() for p in current.rglob("*") if p.is_file()}
    snapshot_files = {p.relative_to(old_root).as_posix() for p in old_root.rglob("*") if p.is_file()} - {"guarded_cloud_service.cpp", "guarded_local_session_repository.cpp", "tuned_depot_installer.cpp", "observed_http_client.cpp", "space_aware_install_runner.cpp", "guarded_cm_app_details.cpp"}
    if current_files != snapshot_files: raise ValueError("Steam source file set changed; use a full rebuild")
    changed = [name for name in current_files if (current / name).read_bytes() != (old_root / name).read_bytes()]
    old = (old_root / "runtime.cpp").read_bytes(); new = (current / "runtime.cpp").read_bytes()
    replacement_text = NEW.replace(b"\n", b"\r\n") if b"\r\n" in old else NEW
    if changed != ["runtime.cpp"] or old.count(OLD) != 1 or old.replace(OLD, replacement_text) != new:
        raise ValueError("More than the reviewed runtime guard changed; use a full rebuild")
    for path in (ROOT / "src").iterdir():
        staged = SNAP / path.name
        if path.is_file() and path.suffix in (".cpp", ".hpp") and path.name != "ps5.cpp" and staged.is_file() and path.read_bytes() != staged.read_bytes():
            raise ValueError("A desktop source changed; use a full rebuild: " + path.name)
    env = os.environ.copy()
    env["WINE11_EXPERIMENT_GAME_HOST"] = "1"
    run([sys.executable, "tools/prepare-steam-native-experiment.py"], env=env)
    if (ROOT / "build/steam-native-experiment/ps5.cpp").read_bytes() != (SNAP / "ps5.cpp").read_bytes():
        raise ValueError("Native entry point changed; use a full rebuild")
    objects_file = BACKEND / "objects.list"
    objects = [Path(os.fsdecode(x)) for x in objects_file.read_bytes().split(b"\0") if x]
    matches = [p for p in objects if p.parent == BACKEND and p.name.startswith("runtime-") and p.suffix == ".o"]
    session_matches = [p for p in objects if p.parent == BACKEND and p.name.startswith("local_session_repository-") and p.suffix == ".o"]
    if len(matches) != 1 or len(session_matches) != 1 or any(not p.is_file() or p.parent != BACKEND for p in objects):
        raise ValueError("Backend object list is incomplete or ambiguous")
    replacement = BACKEND / ("runtime-refreshed-" + hashlib.sha256(new).hexdigest()[:16] + ".o")
    if replacement.exists(): raise ValueError("Refreshed runtime object already exists")
    session_source = BACKEND / "guarded-session-refresh.cpp"
    session_object = BACKEND / "guarded-session-refresh.o"
    if session_source.exists() or session_object.exists(): raise ValueError("Refreshed session files already exist")
    run([sys.executable, "tools/prepare-steam-session-experiment.py", str(session_source)])
    llvm = Path.home() / ".cache/wine11-llvm-18.1.8/root/usr"
    env["PS5_PAYLOAD_SDK"] = str(SDK); env["LLVM_CONFIG"] = str(llvm / "lib/llvm-18/bin/llvm-config")
    env["LD_LIBRARY_PATH"] = str(llvm / "lib/x86_64-linux-gnu") + ":" + str(llvm / "lib/llvm-18/lib") + ":" + env.get("LD_LIBRARY_PATH", "")
    flags = ["-std=c++20", "-O2", "-ffunction-sections", "-fdata-sections", "-DPLAYDEN_PLATFORM_PS5=1",
             "-DWINE11_DEDICATED_HOST=1", "-DWINE11_PROBE_CLOUD=1", "-DWINE11_PROBE_DOWNLOADS=1", "-DWINE11_PROBE_NATIVE_STARTUP=1"]
    includes = ["-Ivendor/playden-source/src", "-Ivendor/playden-source/third_party/qrcodegen", "-Ivendor/playden-source/third_party/stb",
                "-Ivendor/playden-source/third_party/lzma-sdk", "-Ivendor/playden-source/third_party/zstd", "-Ivendor/playden-source/third_party/zstd/common", "-I" + str(NETWORK / "include")]
    run([str(SDK / "bin/prospero-clang++"), *flags, *includes, "-c", str(current / "runtime.cpp"), "-o", str(replacement)], env=env)
    run([str(SDK / "bin/prospero-clang++"), *flags, *includes, "-c", str(session_source), "-o", str(session_object)], env=env)
    objects = [replacement if p == matches[0] else session_object if p == session_matches[0] else p for p in objects]
    libraries = [NETWORK / "lib" / name for name in ("libcurl.a", "libmbedtls.a", "libmbedx509.a", "libmbedcrypto.a", "libeverest.a", "libp256m.a")]
    run([str(SDK / "bin/prospero-clang++"), *map(str, objects), "-o", str(BACKEND / "steam-client-refreshed.elf"),
         "-Wl,--gc-sections", "-Wl,--start-group", *map(str, libraries), "-Wl,--end-group"], env=env)
    shutil.copyfile(current / "runtime.cpp", old_root / "runtime.cpp")
    shutil.copyfile(session_source, old_root / "guarded_local_session_repository.cpp")
    run([sys.executable, "tools/hash-steam-native-snapshot.py", "record"])
    temp = BACKEND / "objects.list.refresh"
    temp.write_bytes(b"\0".join(os.fsencode(p) for p in objects) + b"\0")
    os.replace(temp, objects_file)
    (ROOT / "build/steam-backend-remember.log").write_text("PASS: linked Playden real Steam runtime; refreshed saved-session UID guard.\n")
    env.update(WINE11_FRESH_UI="1", WINE11_USE_SNAPSHOT="1", WINE11_PROBE_GAME_HOST="1", WINE11_PROBE_DEDICATED_HOST="1")
    with previous.open("w") as log:
        run(["bash", "tools/probe-steam-native-pie.sh"], env=env, stdout=log, stderr=subprocess.STDOUT)
    print("PASS: refreshed runtime and relinked the complete diagnostic; not deployed")

if __name__ == "__main__": main()
