"""Stage and install the reviewed Skyrim launch fix on Vapor PPSA77382.

The installed manifest identifies every existing package file. This update
changes only eboot.bin, so preflight verifies the manifest and that one file
(instead of asking the console's Web File Manager for 41 separate downloads).
The previous eboot.bin and manifest.json are kept as backups. No game files
are touched. Requires Web File Manager and the isolated Vapor title closed.
"""
import hashlib
import argparse
import json
from pathlib import Path, PurePosixPath
import sys
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parent))
import update_ps5_vapor_test as updater
from install_console import WFM
from probe_ps5_kit_extraction import post, wait


OLD = updater.ROOT / "build/vapor-controls3-isolated/PPSA77382"
NEW = updater.ROOT / "build/vapor-skyrim-isolated/PPSA77382"
STATUS = updater.ROOT / "build/vapor-skyrim-update-status.json"
CHANGED = ["eboot.bin"]
TAG = "vapor-skyrim"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def package_manifest(package):
    manifest = json.loads((package / "manifest.json").read_text())
    actual = {p.relative_to(package).as_posix() for p in package.rglob("*") if p.is_file() and p.name != "manifest.json"}
    if actual != set(manifest["files"]) or any(p.is_symlink() for p in package.rglob("*")):
        raise RuntimeError("Unexpected package inventory")
    if any(sha((package / name).read_bytes()) != digest for name, digest in manifest["files"].items()):
        raise RuntimeError("Local package verification failed")
    return manifest


def remote(name):
    return "/data/homebrew/PPSA77382/" + name


def stage():
    if STATUS.exists():
        raise RuntimeError("Inspect prior update before retrying")
    old = package_manifest(OLD)
    new = package_manifest(NEW)
    if set(old["files"]) != set(new["files"]) or any(
            old["files"][name] != new["files"][name] for name in old["files"] if name not in CHANGED):
        raise RuntimeError("Reviewed update changes more than eboot.bin")
    if old["files"]["eboot.bin"] == new["files"]["eboot.bin"]:
        raise RuntimeError("Reviewed eboot change is missing")
    wfm = WFM("192.168.7.26")
    installed_manifest = wfm.read(remote("manifest.json"))
    if sha(installed_manifest) != sha((OLD / "manifest.json").read_bytes()):
        raise RuntimeError("Installed manifest differs from the reviewed baseline")
    if sha(wfm.read(remote("eboot.bin"))) != old["files"]["eboot.bin"]:
        raise RuntimeError("Installed eboot differs from the reviewed baseline")
    state = {"phase": "staging", "changed": CHANGED, "staged": []}
    with STATUS.open("x") as file:
        json.dump(state, file, indent=2)
    try:
        for name in CHANGED + ["manifest.json"]:
            parent = "/data/homebrew/PPSA77382"
            leaf = PurePosixPath(name).name
            stage_name = leaf + "." + TAG + ".stage"
            backup_name = leaf + ".before-" + TAG + ".bak"
            entries = {entry["name"] for entry in wfm.listing(parent)}
            if stage_name in entries or backup_name in entries:
                raise RuntimeError("Existing stage or backup prevents retry: " + name)
            data = (NEW / name).read_bytes()
            task = post("/api/upload/prepare", {
                "path": parent, "src": stage_name, "total": str(len(data)), "count": "1",
                "rels": stage_name, "sizes": str(len(data)), "overwrite": "0"})["task_id"]
            post("/api/upload-file", data, headers={
                "Content-Type": "application/octet-stream", "X-WFM-Task-ID": str(task),
                "X-WFM-Path": urllib.parse.quote(parent, safe=""),
                "X-WFM-Rel": urllib.parse.quote(stage_name, safe=""),
                "X-WFM-Size": str(len(data)), "X-WFM-Overwrite": "0"})
            post("/api/upload/finish", params={"task_id": task})
            wait(task)
            if sha(wfm.read(parent + "/" + stage_name)) != sha(data):
                raise RuntimeError("Staged file differs: " + name)
            task = post("/api/chmod", {
                "paths": parent + "/" + stage_name,
                "mode": "0755" if name == "eboot.bin" else "0644", "recursive": "0"})["task_id"]
            wait(task)
            state["staged"].append(name)
            STATUS.write_text(json.dumps(state, indent=2) + "\n")
        state["phase"] = "staged"
        STATUS.write_text(json.dumps(state, indent=2) + "\n")
        print("Vapor update staged and verified; installed app unchanged.")
    except Exception as error:
        state.update(phase="inspect-before-retry", error=str(error))
        STATUS.write_text(json.dumps(state, indent=2) + "\n")
        raise


def apply():
    state = json.loads(STATUS.read_text())
    if state.get("phase") != "staged" or state.get("changed") != CHANGED:
        raise RuntimeError("Update is not in a verified staged state")
    new = package_manifest(NEW)
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list", timeout=5) as response:
        processes = json.load(response).get("processes", [])
    if any(str(process.get("name", "")).lower() in ("payload.elf", "eboot.bin") for process in processes):
        raise RuntimeError("Close Vapor/Wine 11 before applying the update")
    wfm = WFM("192.168.7.26")
    # Recheck both ends immediately before replacing anything. A successful
    # earlier stage does not authorize replacing a subsequently changed app.
    if sha(wfm.read(remote("manifest.json"))) != sha((OLD / "manifest.json").read_bytes()):
        raise RuntimeError("Installed manifest changed since staging")
    for name in CHANGED + ["manifest.json"]:
        if sha(wfm.read(remote(name))) != sha((OLD / name).read_bytes()):
            raise RuntimeError("Installed file changed since staging: " + name)
        if sha(wfm.read(remote(name) + "." + TAG + ".stage")) != sha((NEW / name).read_bytes()):
            raise RuntimeError("Staged file changed: " + name)
        if PurePosixPath(name).name + ".before-" + TAG + ".bak" in {
                entry["name"] for entry in wfm.listing("/data/homebrew/PPSA77382")}:
            raise RuntimeError("Backup appeared since staging: " + name)
    state["phase"] = "applying"
    state["renames"] = []
    STATUS.write_text(json.dumps(state, indent=2) + "\n")
    try:
        for name in CHANGED + ["manifest.json"]:
            leaf = PurePosixPath(name).name
            post("/api/rename", params={"path": remote(name), "name": leaf + ".before-" + TAG + ".bak"})
            state["renames"].append([name, "backup"])
            STATUS.write_text(json.dumps(state, indent=2) + "\n")
            post("/api/rename", params={"path": remote(name) + "." + TAG + ".stage", "name": leaf})
            state["renames"].append([name, "applied"])
            STATUS.write_text(json.dumps(state, indent=2) + "\n")
            if sha(wfm.read(remote(name))) != sha((NEW / name).read_bytes()):
                raise RuntimeError("Update verification failed: " + name)
        state["phase"] = "updated"
        STATUS.write_text(json.dumps(state, indent=2) + "\n")
        print("Vapor update installed; previous files retained as backups.")
    except Exception as error:
        state.update(phase="inspect-before-retry", error=str(error))
        STATUS.write_text(json.dumps(state, indent=2) + "\n")
        raise


def main():
    global OLD, NEW, STATUS, TAG
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old", default="vapor-controls3")
    parser.add_argument("--name", default="vapor-skyrim")
    args = parser.parse_args()
    for value in (args.old, args.name):
        if not value or len(value) > 64 or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-" for c in value):
            parser.error("Build names must contain only lowercase letters, digits and hyphens")
    OLD = updater.ROOT / ("build/" + args.old + "-isolated/PPSA77382")
    NEW = updater.ROOT / ("build/" + args.name + "-isolated/PPSA77382")
    TAG = args.name
    STATUS = updater.ROOT / ("build/" + TAG + "-update-status.json")
    print("Verified local update; changed:", ", ".join(CHANGED))
    print("Old eboot:", package_manifest(OLD)["files"]["eboot.bin"])
    print("New eboot:", package_manifest(NEW)["files"]["eboot.bin"])
    stage()
    apply()
    print("Open Vapor, select Skyrim, choose Controller, and press Play.")


if __name__ == "__main__":
    main()
