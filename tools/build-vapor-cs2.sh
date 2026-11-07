#!/usr/bin/env bash
# Full Vapor application after the pinned source-build prerequisites are prepared.
# Local build/packaging only; no PS5 connection or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
name="${1:?Usage: bash tools/build-vapor-cs2.sh NEW_BUILD_NAME}"
[[ "$name" =~ ^[a-zA-Z0-9][a-zA-Z0-9_-]*$ ]] || { echo 'Invalid build name' >&2; exit 1; }
[[ ! -e "build/$name-review" && ! -e "build/$name-isolated" && ! -e "build/$name-assets" ]] || { echo 'Build output already exists' >&2; exit 1; }
python3 tools/prepare-cs2-runtime.py
python3 tools/refresh-steam-runtime-host.py
cmake --build build/steam-host-dedicated-cmake -j "${BUILD_JOBS:-4}"
export WINE11_EXPERIMENT_GAME_HOST=1 WINE11_EXPERIMENT_UNINSTALL=1 WINE11_PAYLOAD_GAME_CLEANUP=1
export WINE11_TESTED_LINK_PROBE=1 WINE11_USE_SNAPSHOT=1 WINE11_PROBE_DOWNLOADS=1 WINE11_PROBE_CLOUD=1
export WINE11_FRESH_UI=1 WINE11_PROBE_GAME_HOST=1 WINE11_PROBE_DEDICATED_HOST=1
build_native() {
  bash tools/build-steam-native-ui-probe.sh
  bash tools/probe-steam-link.sh
  bash tools/probe-steam-native-pie.sh
}
build_native 2>&1 | tee "build/$name-build.log"
python3 tools/stage-steam-runtime-assets.py "build/$name-assets"
python3 - "$name" <<'PY'
import importlib.util, sys
from pathlib import Path
name = sys.argv[1]
spec = importlib.util.spec_from_file_location('assemble', 'tools/assemble-steam-review.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
module.ASSETS = Path('build/' + name + '-assets').resolve()
sys.argv = ['assemble', 'build/' + name + '-review', 'build/' + name + '-build.log']
module.main()
PY
python3 tools/assemble-steam-isolated-title.py "build/$name-isolated" "build/$name-review/PPSA77381" "build/$name-build.log"
