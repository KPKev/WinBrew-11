"""Prepare an isolated host source for a Wine 11-owned runtime kit.

The helper and derived kit are not installed or qualified by this operation.
The upstream Playden source and its existing kit remain untouched.
"""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[1]
source = root/'build/steam-host-source'
target = root/'build/steam-host-dedicated-source'
if target.exists():
    raise SystemExit('Dedicated host source already exists; refusing to overwrite it')
if not (source/'CMakeLists.txt').is_file():
    raise SystemExit('Build the isolated Wine 11 game-host source first')
shutil.copytree(source, target)
shutil.copy2(root/'src/steam_integration/cs2_runtime_files.hpp', target/'src/cs2_runtime_files.hpp')
shutil.copy2(root/'src/steam_integration/blender_runtime_files.hpp', target/'src/blender_runtime_files.hpp')
changes = {
    'src/runtime/package_installer.h': ('/data/playden/title-rule/allow-list', '/data/wine11/title-rule/allow-list'),
    'src/runtime/kit_trust.h': ('/data/playden/kits/playden-kit', '/data/wine11/kits/playden-kit'),
    # Existing Wine 11 bottles must follow a moved or updated Wine 11 kit's
    # Mono path, just as Playden bottles follow Playden's kit path.
    'src/runtime/game_settings.cpp': (r'"Z:\\data\\playden\\kits\\', r'"Z:\\data\\wine11\\kits\\'),
    # Visual C++ download caches belong to this app, too; do not write into
    # Playden's independently managed redistributable cache.
    'src/homebrew/runtime_host.cpp': (
        'const auto cache = HostPath(config_.package_paths.console_data_root + "/playden/redist").string();',
        'const auto cache = (config_.state_root / "redist").string();'),
}
for name, (old, new) in changes.items():
    path = target/name; text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit('Unexpected upstream source; refusing ambiguous path substitution: ' + name)
    path.write_text(text.replace(old, new))
from prepare_vapor_title_permissions import transform as title_permissions
import prepare_cs2_runtime_installer as cs2
installer = target/'src/runtime/package_installer.cpp'
installer.write_text(cs2.package_source(title_permissions(installer.read_text())))
for name, transform in [('package_installer.h', cs2.package_header), ('install_service.h', cs2.install_service_header)]:
    path = target/'src/runtime'/name
    path.write_text(transform(path.read_text()))
from prepare_vapor_publication_permissions import transform as publication_permissions, transform_coordinator
publication = target/'src/runtime/settings_publication.cpp'
publication.write_text(publication_permissions(publication.read_text()))
coordinator = target/'src/platform/ps5/native_coordinator.cpp'
coordinator.write_text(transform_coordinator(coordinator.read_text()))
from prepare_vapor_title_trash import transform as title_trash
runtime = target/'src/homebrew/runtime_host.cpp'
runtime.write_text(cs2.runtime_host(title_trash(runtime.read_text())))
print('Prepared isolated host source for a Wine 11-owned kit and title list; not deployable.')
