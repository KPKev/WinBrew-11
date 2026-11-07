#!/usr/bin/env python3
"""Review assembly refuses modified base files and stale Steam sources."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("review", root / "tools/assemble-steam-review.py")
review = importlib.util.module_from_spec(spec); spec.loader.exec_module(review)
with tempfile.TemporaryDirectory() as temp:
    base = Path(temp) / "base"; base.mkdir(); review.BASE = base
    helper = b"synthetic helper"; (base / "lapy.elf").write_bytes(helper)
    (base / "lapy-manifest.json").write_text(json.dumps({"target_title": "PPSA77381", "elf_sha256": hashlib.sha256(helper).hexdigest()}))
    (base / "eboot.bin").write_bytes(b"synthetic executable")
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in base.iterdir()}
    (base / "manifest.json").write_text(json.dumps({"project": "Wine 11", "files": files}))
    assert review.verify_base()["files"] == files
    (base / "eboot.bin").write_bytes(b"altered")
    try: review.verify_base()
    except ValueError: pass
    else: raise AssertionError("modified base executable accepted")
    current = Path(temp) / "current"; snapshot = Path(temp) / "snapshot"
    current.mkdir(); snapshot.mkdir()
    (current / "runtime.cpp").write_text("current")
    (snapshot / "runtime.cpp").write_text("current")
    (snapshot / "guarded_cloud_service.cpp").write_text("generated")
    review.verify_current_steam(current, snapshot)
    (current / "runtime.cpp").write_text("changed")
    try: review.verify_current_steam(current, snapshot)
    except ValueError: pass
    else: raise AssertionError("stale source snapshot accepted")
print("PASS: review assembly rejects changed base files and stale sources")
