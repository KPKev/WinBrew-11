"""Reject a snapshot or backend missing the reviewed depot estimate transformation."""
from pathlib import Path
import hashlib
import os
from prepare_steam_depot_estimates import transform

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT_SOURCE = "build/steam-native-experiment/src/steam_integration/guarded_cm_app_details.cpp"

def verify(root=ROOT):
    root = Path(root)
    upstream = root / "vendor/playden-source/src/steam/backend/cm_app_details.cpp"
    source = root / SNAPSHOT_SOURCE
    expected = transform(upstream.read_text(encoding="utf-8")).encode("utf-8")
    if not source.is_file() or source.is_symlink() or source.read_bytes() != expected:
        raise ValueError("Snapshot lacks the reviewed depot-estimate transformation")
    backend = root / "build/steam-snapshot-native-link-probe"
    name = "guarded_cm_app_details-" + hashlib.sha256(SNAPSHOT_SOURCE.encode()).hexdigest()[:8] + ".o"
    obj = backend / name
    entries = [Path(os.fsdecode(x)) for x in (backend / "objects.list").read_bytes().split(b"\0") if x]
    if entries.count(obj) != 1 or not obj.is_file() or obj.is_symlink() or b"wine11_verified_depot_estimates" not in obj.read_bytes():
        raise ValueError("Backend lacks the compiled depot-estimate transformation")

if __name__ == "__main__":
    verify()
    print("PASS: reviewed depot-estimate source and backend object are present")
