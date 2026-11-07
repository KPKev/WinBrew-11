#!/usr/bin/env python3
"""Exercise kit derivation against Playden's real offline validator and fixture."""
import importlib.util
import json
from pathlib import Path
import sys
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "build/steam-host-deps/prospero-win"
sys.path.insert(0, str(UPSTREAM))
from tools import package_playden_kit as kit

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

upstream = load("kit_fixture", UPSTREAM / "tests/test_playden_kit.py")
sys.path.insert(0, str(ROOT / "tools"))
derive = load("wine11_kit", ROOT / "tools/derive-steam-runtime-kit.py")
case = upstream.PlaydenKitTests()
case.setUp()
try:
    case.make_any_title()
    case.add_wine_mono()
    fixture = case.fixture
    spec = json.loads(fixture.spec.read_text())
    spec["kit_id"] = "playden-kit"
    upstream.write(fixture.spec, json.dumps(spec))
    registry = fixture.prefix / "user.reg"
    upstream.write(registry, registry.read_text().replace("hl2-fw1360-v1", "playden-kit"))
    protocol = ROOT / "build/wine11-lapy-source/source/lapy_elevation_protocol.h"
    fixture.protocol.write_bytes(protocol.read_bytes())
    helper_record = json.loads(fixture.helper_manifest.read_text())
    helper_record["protocol_sha256"] = upstream.digest(fixture.protocol)
    upstream.write(fixture.helper_manifest, json.dumps(helper_record))
    case.package()
    fixture.out.chmod(0o700)
    original_manifest = (fixture.out / "manifest.json").read_bytes()
    helper = ROOT / "build/wine11-lapy-source/build/owned_root_helper-FAKE78-rule/lapy.elf"
    helper_manifest = helper.with_name("lapy-manifest.json")
    output = fixture.root / "wine11-kit"
    derive.derive(fixture.out, output, helper, helper_manifest)
    assert (fixture.out / "manifest.json").read_bytes() == original_manifest
    assert fixture.out.stat().st_mode & 0o777 == 0o700
    assert output.stat().st_mode & 0o777 == 0o755
    with mock.patch.dict(kit.REQUIRED_HELPER_SHA256, {"FAKE78*": derive.HELPER_SHA256}), \
         mock.patch.object(kit, "CONSOLE_KITS_ROOT", "/data/wine11/kits"):
        verified = kit.validate_kit(output)
        title = output / verified["variants"][0]["path"]
        assert derive.digest(title / "lapy.elf") == derive.HELPER_SHA256
        (title / "lapy.elf").write_bytes(b"tampered")
        try:
            kit.validate_kit(output)
        except kit.KitError:
            pass
        else:
            raise AssertionError("modified helper was accepted")
    try:
        derive.derive(fixture.out, output, helper, helper_manifest)
    except ValueError:
        pass
    else:
        raise AssertionError("existing output was replaced")
    print("PASS: derived kit validates, preserves original, and rejects tampering and overwrite")
finally:
    case.tearDown()
