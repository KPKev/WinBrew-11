#!/usr/bin/env python3
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("copykit", root / "tools/copy-playden-kit-readonly.py")
copykit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(copykit)
payload = b"verified runtime"
document = {"kit_id": "playden-kit", "files": [{"path": "runtime/test.bin", "size": len(payload),
             "sha256": hashlib.sha256(payload).hexdigest(), "mode": "0755"}]}
raw = json.dumps(document).encode()
copykit.OBSERVED_MANIFEST = hashlib.sha256(raw).hexdigest()

class Reader:
    def __init__(self, data): self.data = data
    def read(self, path, maximum):
        data = raw if path.endswith("/manifest.json") else self.data
        assert len(data) <= maximum
        return data

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "kit"
    copykit.copy(Reader(payload), output)
    assert (output / "runtime/test.bin").read_bytes() == payload
    assert (output / "manifest.json").read_bytes() == raw
    try: copykit.copy(Reader(payload), output)
    except ValueError: pass
    else: raise AssertionError("existing output accepted")
    try: copykit.copy(Reader(b"bad"), Path(directory) / "bad")
    except ValueError: pass
    else: raise AssertionError("bad file accepted")
    assert not (Path(directory) / "bad/manifest.json").exists()
    partial = Path(directory) / "bad/.wine11-copy-parts" / hashlib.sha256(b"runtime/test.bin").hexdigest()
    partial.write_bytes(payload[:5])  # an interrupted file transfer
    copykit.copy(Reader(payload), Path(directory) / "bad")
    assert (Path(directory) / "bad/runtime/test.bin").read_bytes() == payload
    assert not (Path(directory) / "bad/.wine11-copy-state").exists()
    document["files"][0]["path"] = "../outside"
    unsafe = json.dumps(document).encode()
    copykit.OBSERVED_MANIFEST = hashlib.sha256(unsafe).hexdigest()
    try: copykit.entries(unsafe)
    except ValueError: pass
    else: raise AssertionError("unsafe path accepted")
print("PASS: read-only kit copying validates file data, paths, modes, and output")
