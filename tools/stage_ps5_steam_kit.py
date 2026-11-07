"""Stage the verified experimental kit in a new isolated PS5 folder.

This does not install or launch it, and never modifies existing folders. On any
failure it leaves its unique staging folder for inspection and never retries.
"""
import hashlib
import http.client
import json
from pathlib import Path
import time
import urllib.parse
import uuid
import zipfile
from probe_ps5_kit_extraction import post

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = Path.home() / ".cache/wine11-dedicated-kit-0.13-transport.zip"
EXPECTED = "5593ae93e89b6e72db16e605d2dc382bc96161438eec70c82ae9d94e175007a3"
STATUS = ROOT / "build/steam-kit-stage-status.json"
HOST = "192.168.7.26"

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        while block := f.read(1024 * 1024): h.update(block)
    return h.hexdigest()

def main():
    if STATUS.exists(): raise RuntimeError("Prior stage record exists; inspect before running again")
    if not ARCHIVE.is_file() or digest(ARCHIVE) != EXPECTED: raise RuntimeError("Transport ZIP differs")
    with zipfile.ZipFile(ARCHIVE) as z:
        manifest = json.loads(z.read("playden-kit/manifest.json"))
    files = {x["path"]: x for x in manifest["files"]}
    if len(files) != 4348: raise RuntimeError("Unexpected kit inventory")
    sizes = sum(x["size"] for x in files.values())
    space = post("/api/space", params={"path": "/data"})["spaces"]
    available = next(x["free"] for x in space if x["path"] == "/data")
    if available < sizes + ARCHIVE.stat().st_size + 5 * 1024**3: raise RuntimeError("Insufficient free console space")
    name = "wine11-kit-stage-" + uuid.uuid4().hex[:12]; remote = "/data/" + name
    if any(e["name"] == name for e in post("/api/list", params={"path": "/data"})["entries"]):
        raise RuntimeError("Staging path already exists")
    state = {"path": remote, "phase": "starting", "archive_sha256": EXPECTED,
             "install": False, "existing_files_modified": False, "automatic_retry": False}
    def mark(phase, **fields):
        state.update(phase=phase, **fields); STATUS.write_text(json.dumps(state, indent=2) + "\n")
        print(phase, flush=True)
    def wait(task, phase, limit=600):
        mark(phase, task_id=task)
        deadline = time.monotonic() + limit
        while time.monotonic() < deadline:
            item = next((x for x in post("/api/tasks")["tasks"] if x["id"] == task), None)
            if not item: raise RuntimeError("Task disappeared; inspect console before any retry")
            if item["state"] in ("done", "completed"): return
            if item["state"] not in ("running", "queued", "pending"): raise RuntimeError("Console task failed: " + str(item)[:300])
            time.sleep(1)
        raise RuntimeError("Task completion unknown; inspect console before any retry")
    with STATUS.open("x", encoding="utf-8") as f: json.dump(state, f)
    try:
        post("/api/mkdir", params={"path": "/data", "name": name}); mark("folder-created")
        size = ARCHIVE.stat().st_size
        task = post("/api/upload/prepare", {"path": remote, "src": "kit.zip", "total": str(size), "count": "1",
                    "rels": "kit.zip", "sizes": str(size), "overwrite": "0"})["task_id"]
        mark("uploading", task_id=task)
        connection = http.client.HTTPConnection(HOST, 8888, timeout=120)
        connection.putrequest("POST", "/api/upload-file")
        for key, value in {"Content-Type": "application/octet-stream", "Content-Length": str(size), "X-WFM-Task-ID": str(task),
                           "X-WFM-Path": urllib.parse.quote(remote, safe=""), "X-WFM-Rel": "kit.zip",
                           "X-WFM-Size": str(size), "X-WFM-Overwrite": "0"}.items(): connection.putheader(key, value)
        connection.endheaders()
        with ARCHIVE.open("rb") as f:
            while block := f.read(1024 * 1024): connection.send(block)
        response = connection.getresponse(); reply = json.loads(response.read()); connection.close()
        if response.status != 200 or not reply.get("ok"): raise RuntimeError("Upload was not accepted")
        post("/api/upload/finish", params={"task_id": task}); wait(task, "upload-finishing")
        task = post("/api/extract", {"paths": remote + "/kit.zip", "destination": remote, "separate": "0", "overwrite": "0"})["task_id"]
        wait(task, "extracting")
        kit = remote + "/playden-kit"
        task = post("/api/chmod", {"paths": kit, "mode": "0755", "recursive": "1"})["task_id"]
        wait(task, "setting-directory-and-executable-modes")
        ordinary = [kit + "/" + name for name, entry in files.items() if entry["mode"] == "0644"] + [kit + "/manifest.json"]
        for index in range(0, len(ordinary), 100):
            batch = ordinary[index:index+100]
            task = post("/api/chmod", {"paths": "\n".join(batch), "mode": "0644", "recursive": "0"})["task_id"]
            wait(task, "setting-regular-file-modes", limit=120)
        mark("checking-inventory", task_id=None)
        pending = [(kit, "")]; seen = set()
        while pending:
            directory, prefix = pending.pop()
            for entry in post("/api/list", params={"path": directory})["entries"]:
                part = entry["name"]
                if not part or part in (".", "..") or "/" in part or "\\" in part: raise RuntimeError("Unsafe console entry")
                name = prefix + part
                if entry["type"] == "d":
                    if entry["mode"] & 0o777 != 0o755: raise RuntimeError("Directory mode mismatch: " + name)
                    pending.append((directory + "/" + part, name + "/"))
                elif entry["type"] == "-":
                    expected = files.get(name)
                    if name == "manifest.json":
                        if entry["mode"] & 0o777 != 0o644: raise RuntimeError("Manifest mode mismatch")
                    elif (not expected or entry["size"] != expected["size"] or
                          entry["mode"] & 0o777 != int(expected["mode"], 8)):
                        raise RuntimeError("Kit entry mismatch: " + name)
                    seen.add(name)
                else: raise RuntimeError("Unexpected console entry type: " + name)
        if seen != set(files) | {"manifest.json"}: raise RuntimeError("Console kit inventory differs")
        mark("staged-inventory-checked", files=len(files), task_id=None,
             note="Not installed; console-side content hashing and native admission still required")
    except Exception as exc:
        mark("failed-inspect-before-retry", error=str(exc)); raise

if __name__ == "__main__": main()
