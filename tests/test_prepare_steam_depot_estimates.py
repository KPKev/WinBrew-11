import importlib.util
from pathlib import Path
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('depot_estimates', root / 'tools/prepare_steam_depot_estimates.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
source = (root / 'vendor/playden-source/src/steam/backend/cm_app_details.cpp').read_text(encoding='utf-8')
assert module.transform(source).count('bool install_known = true, download_known = true;') == 1
for invalid in (source.replace(module.OLD, ''), source + module.OLD):
    try: module.transform(invalid)
    except ValueError: pass
    else: raise AssertionError('Unexpected source must be rejected')

# Package verification refuses stale source, absent object, and object lists
# that do not contain the generated source's exact backend object.
import hashlib, sys, tempfile
sys.path.insert(0, str(root / "tools"))
from verify_steam_depot_snapshot import verify, SNAPSHOT_SOURCE
with tempfile.TemporaryDirectory() as temp:
    base = Path(temp)
    upstream = base / "vendor/playden-source/src/steam/backend/cm_app_details.cpp"
    upstream.parent.mkdir(parents=True); upstream.write_text(source, encoding="utf-8")
    snapshot = base / SNAPSHOT_SOURCE
    snapshot.parent.mkdir(parents=True); snapshot.write_bytes(module.transform(source).encode("utf-8"))
    backend = base / "build/steam-snapshot-native-link-probe"; backend.mkdir(parents=True)
    name = "guarded_cm_app_details-" + hashlib.sha256(SNAPSHOT_SOURCE.encode()).hexdigest()[:8] + ".o"
    obj = backend / name; obj.write_bytes(b"synthetic wine11_verified_depot_estimates")
    listing = backend / "objects.list"; listing.write_bytes(str(obj).encode() + b"\0")
    verify(base)
    for bad in (b"wrong", b""):
        listing.write_bytes(bad)
        try: verify(base)
        except ValueError: pass
        else: raise AssertionError("Unlisted backend object was accepted")
    listing.write_bytes(str(obj).encode() + b"\0")
    snapshot.write_text("stale")
    try: verify(base)
    except ValueError: pass
    else: raise AssertionError("Stale transformation was accepted")
