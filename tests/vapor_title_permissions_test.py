from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_vapor_title_permissions import transform
source=Path('vendor/playden-source/src/runtime/package_installer.cpp').read_text()
out=transform(source)
a=out.index('bool TitleDirectoriesPublished(const fs::path& title) {');b=out.index('\n}  // namespace',a)
assert out[a:b].count('(info.st_mode & 07777) != 0777') == 2
assert '::chmod(directory.c_str(), 0777)' in out[a:b]
assert out[:a] == source[:a]
assert 'private_profile_file ? 0600' in out
try:transform(out)
except ValueError:pass
else:raise AssertionError('Must reject repeated or ambiguous patch')
print('PASS: isolated directory policy and private file preservation')
