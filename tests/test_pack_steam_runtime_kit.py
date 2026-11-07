#!/usr/bin/env python3
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("kit_pack", root / "tools/pack-steam-runtime-kit.py")
pack = importlib.util.module_from_spec(spec); spec.loader.exec_module(pack)
with tempfile.TemporaryDirectory() as temporary:
    base = Path(temporary); source = base / "kit"; source.mkdir(mode=0o755)
    (source / "bin").mkdir(mode=0o755); (source / "empty").mkdir(mode=0o755)
    binary = source / "bin/tool"; binary.write_bytes(b"runtime"); binary.chmod(0o755)
    manifest = {"files": [{"path": "bin/tool", "mode": "0755", "size": 7, "sha256": pack.sha(binary)}]}
    (source / "manifest.json").write_text(json.dumps(manifest)); (source / "manifest.json").chmod(0o644)
    upstream = base / "release.zip"
    with zipfile.ZipFile(upstream, "w") as release:
        release.writestr(pack.RELEASE_PREFIX + "LICENSES/example", "license")
        release.writestr(pack.RELEASE_PREFIX + "README.md", "readme")
    pack.ORIGINAL_ZIP_SHA256 = pack.sha(upstream); pack.DERIVED_MANIFEST_SHA256 = pack.sha(source / "manifest.json")
    output = base / "out.zip"; pack.pack(source, upstream, output)
    with zipfile.ZipFile(output) as z:
        assert z.read("playden-kit/bin/tool") == b"runtime"
        assert z.read("release-notices/LICENSES/example") == b"license"
        assert z.getinfo("playden-kit/bin/tool").external_attr >> 16 & 0o777 == 0o755
        assert "playden-kit/empty/" in z.namelist()
    try: pack.pack(source, upstream, output)
    except ValueError: pass
    else: raise AssertionError("Existing output was replaced")
    binary.write_bytes(b"tamper!")
    try: pack.pack(source, upstream, base / "bad.zip")
    except ValueError: pass
    else: raise AssertionError("Modified kit was packaged")
    assert not (base / "bad.zip").exists()
print("PASS: transport preserves permissions, empty folders and notices; rejects modification and overwrite")
