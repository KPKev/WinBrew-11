"""Install one isolated experimental test title; never update existing titles."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import sqlite3
import sys
import tempfile
import time
import urllib.parse
from install_console import WFM
from probe_ps5_kit_extraction import post

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "build/steam-isolated-title-candidate/PPSA77382"
STATUS = ROOT / "build/steam-test-title-install-status.json"
TITLE = "PPSA77382"
DEST = "/data/homebrew/" + TITLE

def sha(data): return hashlib.sha256(data).hexdigest()

def main():
    if STATUS.exists(): raise RuntimeError("An earlier test-title attempt exists; inspect it before continuing")
    manifest = json.loads((SOURCE / "manifest.json").read_text())
    names = {p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*") if p.is_file() and p.name != "manifest.json"}
    if (names != set(manifest["files"]) or any(p.is_symlink() for p in SOURCE.rglob("*")) or
        any(sha((SOURCE / name).read_bytes()) != manifest["files"][name] for name in names)):
        raise RuntimeError("Test package inventory or hashes differ")
    for name in names:
        p = PurePosixPath(name)
        if p.is_absolute() or str(p) != name or any(x in (".", "..") for x in p.parts) or "\\" in name:
            raise RuntimeError("Unsafe package path")
    param = json.loads((SOURCE / "sce_sys/param.json").read_text())
    helper = json.loads((SOURCE / "lapy-manifest.json").read_text())
    if param.get("titleId") != TITLE or helper.get("target_title") != TITLE or manifest.get("console_tested") is not False:
        raise RuntimeError("Unexpected test title identity")
    wfm = WFM("192.168.7.26", 8888)
    for parent in ("/data/homebrew", "/user/app", "/user/appmeta"):
        if any(e["name"] == TITLE for e in wfm.listing(parent)): raise RuntimeError("Test title already exists on console")
    entries = {e["name"] for e in wfm.listing("/system_data/priv/mms")}
    with tempfile.TemporaryDirectory(prefix="wine11-title-preflight-") as temp:
        for name in ("app.db", "app.db-wal"):
            if name in entries: (Path(temp) / name).write_bytes(wfm.read("/system_data/priv/mms/" + name))
        db = sqlite3.connect("file:" + str((Path(temp) / "app.db").resolve()) + "?mode=ro", uri=True)
        try:
            if db.execute("select 1 from tbl_contentinfo where titleId=?", (TITLE,)).fetchone():
                raise RuntimeError("Test title is already registered")
        finally: db.close()
    state = {"title": TITLE, "destination": DEST, "phase": "starting", "launched": False,
             "existing_titles_modified": False, "automatic_retry": False}
    def mark(phase, **extra):
        state.update(phase=phase, **extra); STATUS.write_text(json.dumps(state, indent=2) + "\n")
        print(phase, flush=True)
    def upload(selected):
        sizes = [len((SOURCE / name).read_bytes()) for name in selected]
        task = post("/api/upload/prepare", {"path": DEST, "src": selected[0], "total": str(sum(sizes)),
                    "count": str(len(selected)), "rels": "\n".join(selected), "sizes": "\n".join(map(str, sizes)), "overwrite": "0"})["task_id"]
        mark("uploading", task_id=task)
        for name, size in zip(selected, sizes):
            post("/api/upload-file", (SOURCE/name).read_bytes(), headers={"Content-Type":"application/octet-stream",
                 "X-WFM-Task-ID":str(task), "X-WFM-Path":urllib.parse.quote(DEST,safe=""),
                 "X-WFM-Rel":urllib.parse.quote(name,safe=""), "X-WFM-Size":str(size), "X-WFM-Overwrite":"0"})
        post("/api/upload/finish", params={"task_id":task})
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            row=next((t for t in post("/api/tasks")["tasks"] if t["id"]==task),None)
            if not row or row["state"] in ("done","completed"): break
            if row["state"] not in ("running","queued","pending"): raise RuntimeError("Upload task failed")
            time.sleep(.5)
        else: raise RuntimeError("Upload remains active; inspect before retry")
        for name in selected:
            if sha(wfm.read(DEST+"/"+name)) != sha((SOURCE/name).read_bytes()):
                raise RuntimeError("Uploaded file differs: "+name)
    with STATUS.open("x") as f: json.dump(state,f,indent=2)
    try:
        post("/api/mkdir", params={"path":"/data/homebrew","name":TITLE}); mark("folder-created")
        all_names = sorted(names | {"manifest.json"})
        dirs = sorted({str(parent) for name in all_names for parent in PurePosixPath(name).parents if str(parent) != "."}, key=lambda p:(p.count("/"),p))
        for directory in dirs:
            p=PurePosixPath(directory)
            post("/api/mkdir", params={"path":DEST if str(p.parent)=="." else DEST+"/"+str(p.parent),"name":p.name})
        upload([n for n in all_names if n != "sce_sys/param.json"])
        task=post("/api/chmod",{"paths":DEST,"mode":"0755","recursive":"1"})["task_id"]
        mark("setting-permissions",task_id=task)
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            row=next((t for t in post("/api/tasks")["tasks"] if t["id"]==task),None)
            if not row or row["state"] in ("done","completed"): break
            if row["state"] not in ("running","queued","pending"): raise RuntimeError("Permission task failed")
            time.sleep(.5)
        else: raise RuntimeError("Permission task remains active")
        for directory in [""]+dirs:
            entries=wfm.listing(DEST+("/"+directory if directory else ""))
            for item in entries:
                if item["mode"] & 0o777 != 0o755: raise RuntimeError("Package permission not confirmed")
        mark("resources-verified")
        # Registration metadata is published only after every other file is verified.
        upload(["sce_sys/param.json"])
        mark("published",task_id=None,note="Isolated test title; not launched or console-qualified")
    except Exception as exc:
        mark("failed-inspect-before-retry",error=str(exc)); raise

if __name__ == "__main__": main()
