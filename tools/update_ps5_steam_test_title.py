"""Stage or apply an update to the isolated PS5 Steam test title only.

No credentials or game files are read, moved or deleted. The current executable
and package manifest are retained as backups. Never automatically retry a
partially completed update.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
import urllib.parse
import urllib.request
from install_console import WFM
from probe_ps5_kit_extraction import post

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / "build/steam-isolated-title-candidate/PPSA77382"
NEW = ROOT / "build/steam-reference-v2-isolated/PPSA77382"
STATUS = ROOT / "build/steam-test-title-update-status.json"
DEST = "/data/homebrew/PPSA77382"
EXE_STAGE = "eboot.wine11-update.stage"
MANIFEST_STAGE = "manifest.wine11-update.stage"
EXE_BACKUP = "eboot.before-wine11-update.bak"
MANIFEST_BACKUP = "manifest.before-wine11-update.bak"

def sha(data): return hashlib.sha256(data).hexdigest()
def remote(wfm, name): return sha(wfm.read(DEST + "/" + name))

def wait(task):
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        row = next((x for x in post("/api/tasks")["tasks"] if x["id"] == task), None)
        if not row or row["state"] in ("done", "completed"): return
        if row["state"] not in ("running", "queued", "pending"): raise RuntimeError("Console task failed")
        time.sleep(.5)
    raise RuntimeError("Console task is still running; inspect before continuing")

def packages():
    manifests = []
    for package in (OLD, NEW):
        manifest = json.loads((package / "manifest.json").read_text())
        actual = {p.relative_to(package).as_posix() for p in package.rglob("*") if p.is_file() and p.name != "manifest.json"}
        if actual != set(manifest["files"]) or any(p.is_symlink() for p in package.rglob("*")):
            raise RuntimeError("Unexpected package inventory")
        if any(sha((package / name).read_bytes()) != digest for name, digest in manifest["files"].items()):
            raise RuntimeError("Package hash differs")
        if json.loads((package / "sce_sys/param.json").read_text()).get("titleId") != "PPSA77382":
            raise RuntimeError("Unexpected title")
        manifests.append(manifest)
    if set(manifests[0]["files"]) != set(manifests[1]["files"]): raise RuntimeError("Package file set changed")
    changed = {name for name in manifests[0]["files"] if manifests[0]["files"][name] != manifests[1]["files"][name]}
    if changed != {"eboot.bin"}: raise RuntimeError("Update includes unexpected resource changes")
    return manifests

def stage():
    if STATUS.exists(): raise RuntimeError("Inspect the previous update record before continuing")
    old, new = packages(); wfm = WFM("192.168.7.26", 8888)
    entries = {e["name"] for e in wfm.listing(DEST)}
    if any(n in entries for n in (EXE_STAGE, MANIFEST_STAGE, EXE_BACKUP, MANIFEST_BACKUP)):
        raise RuntimeError("Update staging or backup path already exists")
    for name, expected in old["files"].items():
        if remote(wfm, name) != expected: raise RuntimeError("Installed test file changed: " + name)
    if remote(wfm, "manifest.json") != sha((OLD / "manifest.json").read_bytes()):
        raise RuntimeError("Installed manifest changed")
    state = {"phase":"staging", "destination":DEST, "new_eboot_sha256":new["files"]["eboot.bin"],
             "new_manifest_sha256":sha((NEW / "manifest.json").read_bytes()), "existing_files_modified":False}
    with STATUS.open("x") as file: json.dump(state, file, indent=2)
    try:
        items = [(EXE_STAGE, (NEW / "eboot.bin").read_bytes()), (MANIFEST_STAGE, (NEW / "manifest.json").read_bytes())]
        task = post("/api/upload/prepare", {"path":DEST,"src":items[0][0],"total":str(sum(len(v) for _,v in items)),
                    "count":"2","rels":"\n".join(n for n,_ in items),"sizes":"\n".join(str(len(v)) for _,v in items),"overwrite":"0"})["task_id"]
        for name,data in items:
            post("/api/upload-file", data, headers={"Content-Type":"application/octet-stream", "X-WFM-Task-ID":str(task),
                 "X-WFM-Path":urllib.parse.quote(DEST,safe=""), "X-WFM-Rel":name,
                 "X-WFM-Size":str(len(data)), "X-WFM-Overwrite":"0"})
        post("/api/upload/finish",params={"task_id":task}); wait(task)
        for name,data in items:
            if remote(wfm,name) != sha(data): raise RuntimeError("Staged file differs: "+name)
        task=post("/api/chmod",{"paths":DEST+"/"+EXE_STAGE+"\n"+DEST+"/"+MANIFEST_STAGE,"mode":"0755","recursive":"0"})["task_id"]
        wait(task)
        entries={e["name"]:e for e in wfm.listing(DEST)}
        if any(entries[n]["mode"] & 0o777 != 0o755 for n,_ in items): raise RuntimeError("Staged modes not confirmed")
        state["phase"]="staged"; STATUS.write_text(json.dumps(state,indent=2)+"\n")
        print("Update staged and verified; the running app has not been changed.")
    except Exception as ex:
        state.update(phase="inspect-before-retry",error=str(ex)); STATUS.write_text(json.dumps(state,indent=2)+"\n"); raise

def apply():
    state=json.loads(STATUS.read_text())
    if state.get("phase")!="staged" or state.get("destination")!=DEST: raise RuntimeError("Update is not in a verified staged state")
    old,new=packages(); wfm=WFM("192.168.7.26",8888)
    with urllib.request.urlopen("http://192.168.7.26:8084/processes_list",timeout=5) as response:
        processes=json.load(response).get("processes",[])
    if any(str(p.get("name","")).lower()=="eboot.bin" for p in processes):
        raise RuntimeError("An app is running; close the test app before applying the update")
    for name,expected in (("eboot.bin",old["files"]["eboot.bin"]),("manifest.json",sha((OLD/"manifest.json").read_bytes())),
                          (EXE_STAGE,new["files"]["eboot.bin"]),(MANIFEST_STAGE,sha((NEW/"manifest.json").read_bytes()))):
        if remote(wfm,name)!=expected: raise RuntimeError("Console file changed: "+name)
    entries={e["name"] for e in wfm.listing(DEST)}
    if EXE_BACKUP in entries or MANIFEST_BACKUP in entries: raise RuntimeError("Backup path already exists")
    try:
        state["phase"]="applying"; STATUS.write_text(json.dumps(state,indent=2)+"\n")
        for source,target in (("eboot.bin",EXE_BACKUP),(EXE_STAGE,"eboot.bin"),
                              ("manifest.json",MANIFEST_BACKUP),(MANIFEST_STAGE,"manifest.json")):
            post("/api/rename",params={"path":DEST+"/"+source,"name":target})
            state["last_rename"]=[source,target]; STATUS.write_text(json.dumps(state,indent=2)+"\n")
        if remote(wfm,"eboot.bin")!=new["files"]["eboot.bin"] or remote(wfm,"manifest.json")!=state["new_manifest_sha256"]:
            raise RuntimeError("Updated files do not match")
        state.update(phase="updated",existing_files_modified=True); STATUS.write_text(json.dumps(state,indent=2)+"\n")
        print("Updated isolated test title; original executable and manifest retained as backups.")
    except Exception as ex:
        state.update(phase="inspect-before-retry",error=str(ex)); STATUS.write_text(json.dumps(state,indent=2)+"\n"); raise

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("phase",choices=("stage","apply")); args=parser.parse_args()
    stage() if args.phase=="stage" else apply()
