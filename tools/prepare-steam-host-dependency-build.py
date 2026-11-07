"""Prepare an isolated, experimental PS5 build of Playden's game-host libraries.

The local copy records the console-tested archive hashes; the upstream lock and
checkout are untouched. This is a build probe, not a deployable title.
"""
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
upstream = root / 'vendor/playden-source'
out = root / 'build/steam-host-source'
if out.exists():
    raise SystemExit('Isolated host source already exists; preserve it rather than overwrite it.')
subprocess.run(['python3', str(root/'tools/verify-steam-tested-inputs.py')], check=True)
subprocess.run(['python3', str(root/'tools/prepare-steam-runtime-host-experiment.py')], check=True)
if subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'], text=True).strip() != '9f5a7cbebeaf07fa3ea5324dce5ad8f374ca0aec':
    raise SystemExit('Unexpected Playden revision')
subprocess.run(['git','-C',str(upstream),'diff','--quiet'], check=True)
for name in ('src', 'cmake'):
    shutil.copytree(upstream/name, out/name)
# Preserve the pinned upstream source; patch only this isolated host copy.
from prepare_steam_uid_guard import apply as apply_uid_guard
apply_uid_guard(out/'src/platform/effective_uid.cpp')
from prepare_steam_depot_estimates import apply as apply_depot_estimates
apply_depot_estimates(out/'src/steam/backend/cm_app_details.cpp')
(out/'tools').mkdir(parents=True)
shutil.copy2(upstream/'tools/prospero_source.py', out/'tools/prospero_source.py')
(out/'third_party').mkdir()
for name in ('utf8proc','sqlite3','miniz','lzma-sdk','zstd','stb','qrcodegen'):
    shutil.copytree(upstream/'third_party'/name, out/'third_party'/name)
shutil.copy2(root/'build/steam-native-experiment/runtime-host/runtime_host.cpp', out/'src/homebrew/runtime_host.cpp')
for name in ('runtime_install_id.cpp','runtime_install_id.hpp'):
    shutil.copy2(root/'src/steam_integration'/name, out/'src'/name)
lock = json.loads((upstream/'dependencies.lock.json').read_text())
tested = json.loads((root/'tools/steam-network-tested-inputs.json').read_text())['files']
for name in lock['steam_network_probe']['ps5_archives']:
    lock['steam_network_probe']['ps5_archives'][name] = tested['lib/'+name]
(out/'dependencies.lock.json').write_text(json.dumps(lock, indent=2)+'\n')
ui = (upstream/'cmake/HomebrewUI.cmake').read_text()
start = ui.index('add_library(playden_steam_cm_codec STATIC')
end = ui.index('foreach(font_file', start)
host_start = ui.index('add_library(playden_homebrew_runtime_host STATIC', end)
host_end = ui.index('add_library(playden_homebrew_artwork STATIC', host_start)
(out/'cmake/WineHostProbe.cmake').write_text(ui[start:end] + '\n' + ui[host_start:host_end] + '\n' +
    'target_sources(playden_homebrew_runtime_host PRIVATE "${CMAKE_SOURCE_DIR}/src/runtime_install_id.cpp")\n')
(out/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(Wine11HostProbe LANGUAGES C CXX)
set(PLAYDEN_PLATFORM_PS5 ON)
set(BUILD_TESTING OFF)
set(PLAYDEN_STB_IMAGE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/stb")
file(READ "${CMAKE_SOURCE_DIR}/dependencies.lock.json" playden_dependency_lock)
include(cmake/PlaydenCore.cmake)
include(cmake/SteamBackend.cmake)
include(cmake/ProsperoRuntime.cmake)
include(cmake/SelfUpdate.cmake)
include(cmake/WineHostProbe.cmake)
''')
print('Prepared isolated Wine 11 host-library build; local archive pins are experimental.')
