"""Add gated descriptor-relative uninstall calls to Playden's PS5 gateway."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / 'vendor/playden-source/src/platform/ps5/file_shims.c').read_text(encoding='utf-8')
anchor = '#include <sys/types.h>'
if source.count(anchor) != 1 or source.count('static int directory_call(') != 1:
    raise SystemExit('Pinned file shims changed; refusing ambiguous transformation')
source = source.replace(anchor, anchor + '\n#include <sys/stat.h>', 1)
source += '''
// Use the same libkernel gateway as the qualified openat and flock wrappers.
// These are enabled only in the unfinished uninstall experiment.
int __wrap_fstatat(int directory, const char* path, struct stat* value, int flags)
{
    return directory_call(SYS_fstatat, directory, path, (long)(uintptr_t)value, flags);
}
int __wrap_unlinkat(int directory, const char* path, int flags)
{
    return directory_call(SYS_unlinkat, directory, path, flags, 0);
}
'''
if len(sys.argv) != 2:
    raise SystemExit('usage: prepare_vapor_file_shims.py OUTPUT_C')
Path(sys.argv[1]).write_text(source, encoding='utf-8', newline='\n')
