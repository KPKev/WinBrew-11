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
import prepare_cs2_runtime_installer_v1 as previous_installer
from prepare_vapor_title_permissions import transform as title_permissions
from prepare_vapor_publication_permissions import transform as publication_permissions, transform_coordinator

root = Path(__file__).resolve().parents[1]
update_hooks = {
    'update_service.h': '''  // Embedders can require their own integration contract in addition to the
  // generic app/title/runtime manifest. Runs after redirects and before parsing.
  std::function<bool(std::string_view, std::string*)> admit_manifest;
''',
    'update_service.cpp': '''  if (config_.admit_manifest && !config_.admit_manifest(*manifest_text, &error))
    return Fail("This release is not compatible with this app integration.", error);
''',
}
for name, hook in update_hooks.items():
    original = (root / 'vendor/playden-source/src/update' / name).read_text()
    target = root / 'build/steam-host-dedicated-source/src/update' / name
    if original.count(hook) != 1 or target.read_text() not in (original, original.replace(hook, '', 1)):
        raise SystemExit('Preserved updater has independent edits: ' + name)
    if target.read_text() != original:
        target.write_text(original)
# The isolated native host owns a preserved source copy. Refresh appinfo only
# if it is either the current source or the known predecessor of this fix.
appinfo = root / 'vendor/playden-source/src/steam/backend/app_info.cpp'
appinfo_copy = root / 'build/steam-host-dedicated-source/src/steam/backend/app_info.cpp'
current_appinfo = appinfo.read_text()
appinfo_fix = '''  // Some Steam launch entries use a single leading slash for a path inside
  // the install directory. Keep all other path validation strict; in
  // particular, do not turn network paths or traversal into valid paths.
  if (launch.executable.size() > 1 && launch.executable[0] == '/' &&
      launch.executable[1] != '/' && launch.executable[1] != '\\\\') {
    if (const auto relative = RelativeGamePath(std::string_view(launch.executable).substr(1)))
      launch.executable = *relative;
  }
'''
if current_appinfo.count(appinfo_fix) != 1:
    raise SystemExit('Unexpected appinfo normalization source')
if appinfo_copy.read_text() not in (current_appinfo, current_appinfo.replace(appinfo_fix, '', 1)):
    raise SystemExit('Preserved appinfo source has independent edits; refusing overwrite')
if appinfo_copy.read_text() != current_appinfo:
    shutil.copy2(appinfo, appinfo_copy)
settings_source = root / 'vendor/playden-source/src/runtime/game_settings.cpp'
settings_copy = root / 'build/steam-host-dedicated-source/src/runtime/game_settings.cpp'
settings_text = settings_source.read_text().replace('Z:\\\\data\\\\playden\\\\kits', 'Z:\\\\data\\\\wine11\\\\kits')
start = settings_text.index('    // Prefer an explicitly advertised DX11 mode')
end = settings_text.index('  }\n  // The row', start)
settings_previous = settings_text[:start] + settings_text[end:]
if settings_copy.read_text() not in (settings_text, settings_previous, settings_source.read_text()):
    raise SystemExit('Preserved settings source has independent edits; refusing overwrite')
if settings_copy.read_text() != settings_text:
    settings_copy.write_text(settings_text)
shutil.copy2(root/'src/steam_integration/cs2_runtime_files.hpp', root/'build/steam-host-dedicated-source/src/cs2_runtime_files.hpp')
shutil.copy2(root/'src/steam_integration/blender_runtime_files.hpp', root/'build/steam-host-dedicated-source/src/blender_runtime_files.hpp')
shutil.copy2(root/'src/steam_integration/blender_shared_release_policy.hpp', root/'build/steam-host-dedicated-source/src/blender_shared_release_policy.hpp')
shutil.copy2(root/'src/steam_integration/blender_shared_upgrade.hpp', root/'build/steam-host-dedicated-source/src/blender_shared_upgrade.hpp')
shutil.copy2(root/'src/steam_integration/blender_shared_enrollment.hpp', root/'build/steam-host-dedicated-source/src/blender_shared_enrollment.hpp')

shutil.copy2(root/'src/steam_integration/blender_artifact_migration.hpp', root/'build/steam-host-dedicated-source/src/blender_artifact_migration.hpp')
subprocess.run([sys.executable, str(root / 'tools/prepare-steam-runtime-host-experiment.py')], check=True)
generated = (root / 'build/steam-native-experiment/runtime-host/runtime_host.cpp').read_text()
old_cache = 'const auto cache = HostPath(config_.package_paths.console_data_root + "/playden/redist").string();'
new_cache = 'const auto cache = (config_.state_root / "redist").string();'
if generated.count(old_cache) != 1:
    raise SystemExit('Unexpected runtime cache source')
target = root / 'build/steam-host-dedicated-source/src/homebrew/runtime_host.cpp'
previous = generated.replace(old_cache, new_cache)
expected = cs2.runtime_host(title_trash(previous))
graphics_start = expected.index('  // Re-publish an existing unspecified graphics launch')
graphics_end = expected.index('  // "Steam features"', graphics_start)
before_graphics = (expected[:graphics_start] + expected[graphics_end:]).replace(
    '!unowned_upgrade && !graphics_upgrade)', '!unowned_upgrade)').replace(
    'if (force || graphics_upgrade)', 'if (force)')
actual = target.read_text()
legacy_only = expected.replace(' || ww::steam_integration::CanMigrateBlenderInlineEditors(target.runtime_number, target.artifacts, c.installer->ExpectedArtifacts(target))', '', 1).replace(
    ' && !(legacy_blender && artifact.artifact_id == "compatibility-title")', '', 1)
pre_migration = legacy_only.replace('#include "blender_artifact_migration.hpp"\n', '', 1).replace(
    '        const bool legacy_blender = ww::steam_integration::CanMigrateLegacyBlenderArtifacts(target.runtime_number, target.artifacts, c.installer->ExpectedArtifacts(target));\n        if (target.artifacts.size() == c.installer->ExpectedArtifacts(target).size() || legacy_blender) {',
    '        if (target.artifacts.size() == c.installer->ExpectedArtifacts(target).size()) {', 1).replace(
    '            same_set = same_set && (found || (legacy_blender && artifact.artifact_id == "compatibility-title"));',
    '            same_set = same_set && found;', 1)
if actual not in (before_graphics, cs2._runtime_host_base(title_trash(previous)), expected, legacy_only, pre_migration, previous_installer.runtime_host(title_trash(previous)), title_trash(previous), previous, previous.replace(RECONCILE, '', 1)):
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
    legacy = getattr(previous_installer, transform.__name__)(original)
    if path.read_text() not in (original, legacy, updated, cs2._package_source_base(original) if name == "package_installer.cpp" else updated):
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
