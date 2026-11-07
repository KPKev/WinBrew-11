"""Correct only observed remaining modes in the isolated PS5 kit staging folder."""
import json
import hashlib
from pathlib import Path
import time
from probe_ps5_kit_extraction import post

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / "build/steam-kit-stage-status.json"
RESULT = ROOT / "build/steam-kit-mode-recovery.json"
MANIFEST = Path.home() / ".cache/wine11-dedicated-kit-0.13/manifest.json"

def main():
    if RESULT.exists(): raise RuntimeError("Recovery record already exists; inspect it")
    previous = json.loads(OLD.read_text())
    path = previous["path"]
    if previous["phase"] != "failed-inspect-before-retry" or not path.startswith("/data/wine11-kit-stage-") or "/" in path[len("/data/"):]:
        raise RuntimeError("Unexpected staging identity or state")
    raw_manifest = MANIFEST.read_bytes()
    if hashlib.sha256(raw_manifest).hexdigest() != "1cd63397e5c560fddbc0f3b8baea3bf87ace1e0ba997a46ee0ed4acf4dac8dfc":
        raise RuntimeError("Local kit manifest differs")
    expected = {x["path"]: x for x in json.loads(raw_manifest)["files"]}
    if len(expected) != 4348: raise RuntimeError("Unexpected kit inventory")
    kit = path + "/playden-kit"; pending = [(kit, "")]; seen = set(); changes = []
    while pending:
        directory, prefix = pending.pop()
        for item in post("/api/list", params={"path": directory})["entries"]:
            part = item["name"]
            if not part or part in (".", "..") or "/" in part or "\\" in part: raise RuntimeError("Unsafe console name")
            name = prefix + part; mode = item["mode"] & 0o777
            if item["type"] == "d":
                if mode != 0o755: raise RuntimeError("Unexpected directory mode: " + name)
                pending.append((directory + "/" + part, name + "/"))
            elif item["type"] == "-":
                entry = expected.get(name)
                if name == "manifest.json": wanted = 0o644
                elif not entry or item["size"] != entry["size"]: raise RuntimeError("Unexpected file: " + name)
                else: wanted = int(entry["mode"], 8)
                if mode != wanted:
                    if mode != 0o755 or wanted != 0o644: raise RuntimeError("Unexpected mode: " + name)
                    changes.append(directory + "/" + part)
                seen.add(name)
            else: raise RuntimeError("Unexpected entry type: " + name)
    if seen != set(expected) | {"manifest.json"}: raise RuntimeError("Kit file inventory differs")
    state = {"path": path, "phase": "correcting", "remaining_initial": len(changes), "corrected": 0,
             "note": "Isolated staging only; content hashes and console execution remain unverified"}
    with RESULT.open("x") as file: json.dump(state, file, indent=2)
    for offset in range(0, len(changes), 50):
        batch = changes[offset:offset+50]
        task = post("/api/chmod", {"paths": "\n".join(batch), "mode": "0644", "recursive": "0"})["task_id"]
        # A missing task record is resolved by directly checking every file;
        # no chmod is repeated if its outcome cannot be observed.
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            row = next((x for x in post("/api/tasks")["tasks"] if x["id"] == task), None)
            if not row or row["state"] in ("done", "completed"): break
            if row["state"] not in ("running", "queued", "pending"): raise RuntimeError("Permission task failed")
            time.sleep(.5)
        else: raise RuntimeError("Permission task is still running; inspect it")
        grouped = {}
        for file in batch:
            directory, name = file.rsplit("/", 1); grouped.setdefault(directory, set()).add(name)
        for directory, names in grouped.items():
            found = {x["name"] for x in post("/api/list", params={"path": directory})["entries"]
                     if x["name"] in names and x["type"] == "-" and x["mode"] & 0o777 == 0o644}
            if found != names: raise RuntimeError("Permission outcome not established; inspect before any retry")
        state["corrected"] += len(batch); RESULT.write_text(json.dumps(state, indent=2) + "\n")
    state["phase"] = "inventory-and-modes-verified"; RESULT.write_text(json.dumps(state, indent=2) + "\n")
    print("PASS: isolated staging inventory and modes verified; corrected", len(changes), "files")

if __name__ == "__main__": main()
