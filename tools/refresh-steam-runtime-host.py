"""Refresh the existing isolated host copy after the reviewed startup overlay.

Accept only the exact generated source or its pre-reconciliation predecessor;
never overwrite independent edits in a preserved build tree.
"""
from pathlib import Path
import subprocess
import sys
import shutil
from prepare_steam_registration_reconcile import RECONCILE
from prepare_vapor_title_trash import transform as title_trash
import prepare_cs2_runtime_installer as cs2
from prepare_vapor_title_permissions import transform as title_permissions
from prepare_vapor_publication_permissions import transform as publication_permissions, transform_coordinator

root = Path(__file__).resolve().parents[1]
shutil.copy2(root/'src/steam_integration/cs2_runtime_files.hpp', root/'build/steam-host-dedicated-source/src/cs2_runtime_files.hpp')
subprocess.run([sys.executable, str(root / 'tools/prepare-steam-runtime-host-experiment.py')], check=True)
generated = (root / 'build/steam-native-experiment/runtime-host/runtime_host.cpp').read_text()
old_cache = 'const auto cache = HostPath(config_.package_paths.console_data_root + "/playden/redist").string();'
new_cache = 'const auto cache = (config_.state_root / "redist").string();'
if generated.count(old_cache) != 1:
    raise SystemExit('Unexpected runtime cache source')
target = root / 'build/steam-host-dedicated-source/src/homebrew/runtime_host.cpp'
previous = generated.replace(old_cache, new_cache)
expected = cs2.runtime_host(title_trash(previous))
actual = target.read_text()
if actual not in (expected, title_trash(previous), previous, previous.replace(RECONCILE, '', 1)):
    raise SystemExit('Preserved dedicated host has independent edits; refusing overwrite')
if actual != expected:
    target.write_text(expected)
for name, transform in [('package_installer.h', cs2.package_header), ('install_service.h', cs2.install_service_header),
                        ('package_installer.cpp', cs2.package_source)]:
    path = root / 'build/steam-host-dedicated-source/src/runtime' / name
    original = (root / 'vendor/playden-source/src/runtime' / name).read_text()
    if name == 'package_installer.h':
        original = original.replace('/data/playden/title-rule/allow-list', '/data/wine11/title-rule/allow-list')
    if name == 'package_installer.cpp': original = title_permissions(original)
    updated = transform(original)
    if path.read_text() not in (original, updated):
        raise SystemExit('Independent installer edits: ' + name)
    path.write_text(updated)
publication = root / 'build/steam-host-dedicated-source/src/runtime/settings_publication.cpp'
original_publication = (root / 'vendor/playden-source/src/runtime/settings_publication.cpp').read_text()
expected_publication = publication_permissions(original_publication)
actual_publication = publication.read_text()
if actual_publication not in (original_publication, expected_publication):
    raise SystemExit('Preserved publication source has independent edits; refusing overwrite')
if actual_publication != expected_publication:
    publication.write_text(expected_publication)
coordinator = root / 'build/steam-host-dedicated-source/src/platform/ps5/native_coordinator.cpp'
original_coordinator = (root / 'vendor/playden-source/src/platform/ps5/native_coordinator.cpp').read_text()
expected_coordinator = transform_coordinator(original_coordinator)
actual_coordinator = coordinator.read_text()
if actual_coordinator not in (original_coordinator, expected_coordinator):
    raise SystemExit('Preserved native coordinator has independent edits; refusing overwrite')
if actual_coordinator != expected_coordinator:
    coordinator.write_text(expected_coordinator)
print('PASS: dedicated host matches registration, cache, and title-retirement overlays')
