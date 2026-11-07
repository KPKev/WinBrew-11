"""Create a Wine 11-specific RuntimeHost source for compile-only exploration.

The bottle identity changes do not solve shared title registration, the global
allow-list, runtime-kit admission or concurrency. This output is not deployable.
"""
from pathlib import Path
from prepare_steam_registration_reconcile import transform as reconcile_registration
root = Path(__file__).resolve().parents[1]
source = root / 'vendor/playden-source/src/homebrew/runtime_host.cpp'
text = source.read_text()
needle = '#include "homebrew/runtime_host.hpp"'
if text.count(needle) != 1 or text.count('ToInstallId(') != 6 or text.count('ParseInstallId(') != 2:
    raise SystemExit('RuntimeHost changed; refusing an ambiguous identity transformation')
text = text.replace(needle, needle + '\n#include "runtime_install_id.hpp"')
text = text.replace('ToInstallId(', 'ww::steam_integration::WineInstallId(')
text = text.replace('ParseInstallId(', 'ww::steam_integration::ParseWineInstallId(')
text = reconcile_registration(text)
dest = root / 'build/steam-native-experiment/runtime-host'
dest.mkdir(parents=True, exist_ok=True)
(dest / 'runtime_host.cpp').write_text(text)
print('Generated isolated Wine 11 runtime-host experiment; not deployable.')
