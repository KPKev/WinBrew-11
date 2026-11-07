from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from prepare_vapor_publication_permissions import transform, transform_coordinator

source = (root / 'vendor/playden-source/src/runtime/settings_publication.cpp').read_text()
updated = transform(source)
assert updated.count('(title_info.st_mode & 07777) != 0777') == 2
assert updated.count('title_info.st_uid != geteuid()') == source.count('title_info.st_uid != geteuid()') == 2
assert updated.count('DirectoryPolicy::kApprovedTree') == source.count('DirectoryPolicy::kApprovedTree')
try:
    transform(updated)
    raise AssertionError('Already-patched or unexpected source must be refused')
except ValueError:
    pass
coordinator = (root / 'vendor/playden-source/src/platform/ps5/native_coordinator.cpp').read_text()
coordinator_updated = transform_coordinator(coordinator)
assert coordinator_updated.count('(info.st_mode & 07777) == 0777') == 1
assert coordinator_updated.count('info.st_uid == geteuid()') == coordinator.count('info.st_uid == geteuid()')
try:
    transform_coordinator(coordinator_updated)
    raise AssertionError('Changed coordinator source must be refused')
except ValueError:
    pass
print('editable title publication checks passed')
