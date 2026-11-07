"""Test the console file manager's ZIP mode handling in a new isolated folder.

Writes only harmless test data to a uniquely named folder. Does not install a
kit, modify existing files, start a game, or retry incomplete console tasks.
"""
import hashlib
import io
import json
from pathlib import Path
import time
import urllib.parse
import urllib.request
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "evidence/steam-kit-extraction-console-probe.json"
BASE = "http://192.168.7.26:8888"

def post(endpoint, data=None, params=None, headers=None):
    url = BASE + endpoint + ("?" + urllib.parse.urlencode(params) if params else "")
    if isinstance(data, dict):
        data = urllib.parse.urlencode(data).encode()
        headers = {**(headers or {}), "Content-Type": "application/x-www-form-urlencoded"}
    with urllib.request.urlopen(urllib.request.Request(url, data=data or b"", headers=headers or {}, method="POST"), timeout=20) as response:
        result = json.load(response)
    if not result.get("ok"): raise RuntimeError(str(result)[:300])
    return result

def wait(task):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        row = next((x for x in post("/api/tasks")["tasks"] if x["id"] == task), None)
        if not row: raise RuntimeError("Task disappeared; inspect before further action")
        if row["state"] in ("done", "completed"): return
        if row["state"] not in ("running", "queued", "pending"): raise RuntimeError(str(row)[:300])
        time.sleep(.5)
    raise RuntimeError("Task did not complete; inspect before further action")

def archive():
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as out:
        for name, mode, content in (("probe/", 0o40755, b""), ("probe/executable", 0o100755, b"harmless mode test\n"),
                                    ("probe/data", 0o100644, b"harmless data test\n")):
            info = zipfile.ZipInfo(name); info.create_system = 3; info.external_attr = mode << 16
            if name.endswith("/"): info.external_attr |= 0x10
            out.writestr(info, content)
    return buf.getvalue()

def main():
    if EVIDENCE.exists(): raise RuntimeError("Inspect existing test before repeating")
    name = "wine11-extract-test-" + uuid.uuid4().hex[:12]; path = "/data/" + name
    entries = post("/api/list", params={"path": "/data"})["entries"]
    if any(x["name"] == name for x in entries): raise RuntimeError("Test path already exists")
    data = archive()
    post("/api/mkdir", params={"path": "/data", "name": name})
    task = post("/api/upload/prepare", {"path": path, "src": "probe.zip", "total": str(len(data)),
                "count": "1", "rels": "probe.zip", "sizes": str(len(data)), "overwrite": "0"})["task_id"]
    post("/api/upload-file", data, headers={"Content-Type": "application/octet-stream", "X-WFM-Task-ID": str(task),
         "X-WFM-Path": urllib.parse.quote(path, safe=""), "X-WFM-Rel": "probe.zip",
         "X-WFM-Size": str(len(data)), "X-WFM-Overwrite": "0"})
    post("/api/upload/finish", params={"task_id": task}); wait(task)
    task = post("/api/extract", {"paths": path + "/probe.zip", "destination": path, "separate": "0", "overwrite": "0"})["task_id"]
    wait(task)
    result = post("/api/list", params={"path": path + "/probe"})["entries"]
    modes = {item["name"]: item["mode"] & 0o777 for item in result if item["name"] in ("data", "executable")}
    if set(modes) != {"data", "executable"}: raise RuntimeError("Expected extracted files are missing")
    evidence = {"test_path": path, "archive_sha256": hashlib.sha256(data).hexdigest(), "observed_modes": modes,
                "expected_modes": {"data": 0o644, "executable": 0o755}, "preserves_modes": modes == {"data": 0o644, "executable": 0o755},
                "contains_only_harmless_test_data": True, "installed_or_modified_existing_files": False}
    with EVIDENCE.open("x", encoding="utf-8") as file: json.dump(evidence, file, indent=2); file.write("\n")
    print(json.dumps(evidence))

if __name__ == "__main__": main()
