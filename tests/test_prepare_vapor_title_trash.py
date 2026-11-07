from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from prepare_vapor_title_trash import transform
from prepare_steam_registration_reconcile import transform as reconcile_registration

root = Path(__file__).resolve().parents[1]
generated = (root / 'vendor/playden-source/src/homebrew/runtime_host.cpp').read_text()
generated = generated.replace('#include "homebrew/runtime_host.hpp"',
                              '#include "homebrew/runtime_host.hpp"\n#include "runtime_install_id.hpp"', 1)
generated = generated.replace('ToInstallId(', 'ww::steam_integration::WineInstallId(')
generated = generated.replace('ParseInstallId(', 'ww::steam_integration::ParseWineInstallId(')
generated = reconcile_registration(generated)
old = 'const auto cache = HostPath(config_.package_paths.console_data_root + "/playden/redist").string();'
new = 'const auto cache = (config_.state_root / "redist").string();'
assert generated.count(old) == 1
source = generated.replace(old, new)
patched = transform(source)
assert patched.count('HostPath("/data/wine11/wine11-games") / ".vapor-title-trash"') == 3
assert 'games->OpenChild(".vapor-title-trash", platform::DirectoryPolicy::kPrivate, true)' in patched
assert 'ManagedDirectory::Open(private_trash->path(), platform::DirectoryPolicy::kApprovedTree)' in patched
assert 'private_info.st_ino != approved_info.st_ino' in patched
assert 'games_info.st_dev != info.st_dev' in patched
assert 'entry == record.title_id + "-" + record.journal->operation_id' in patched
for altered in (source.replace('config_.state_root / "trash"', '', 1), patched):
    try:
        transform(altered)
        raise AssertionError('Changed or already-patched input must be refused')
    except ValueError:
        pass
print('title retirement overlay checks passed')
