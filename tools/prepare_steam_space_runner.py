"""Generate a runner that reports an unavailable free-space query accurately."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root / 'vendor/playden-source/src/steam/backend/steam_install_runner.cpp').read_text()
include = '#include "steam/backend/steam_install_runner.h"'
if source.count(include) != 1: raise SystemExit('Unexpected Steam runner include; refusing ambiguous patch')
source = source.replace(include, include + '\n#include "steam_integration/download_space_status.hpp"', 1)
old = '  result.failure = InstallFailureText(outcome.error);'
new = '''  result.failure = ww::steam_integration::IsSpaceQueryUnavailable(outcome.error, outcome.detail)
      ? ww::steam_integration::kSpaceQueryMessage
      : InstallFailureText(outcome.error);'''
if source.count(old) != 1: raise SystemExit('Unexpected Steam runner source; refusing ambiguous patch')
output = Path(sys.argv[1]) if len(sys.argv) == 2 else root / 'build/steam-native-experiment/src/steam_integration/space_aware_install_runner.cpp'
output.write_text(source.replace(old,new), encoding='utf-8', newline='\n')
