#!/usr/bin/env python3
import pathlib,shutil,difflib,json
repo=pathlib.Path.cwd();work=pathlib.Path.home()/'counterstrike-wine019-work'
source=work/'wine-source';p=source/'dlls/ntdll/unix/virtual.c';before=p.read_text();s=before
anchor='#define PS5_HIGH_RESERVE_MAX ((size_t)512 << 30)'
assert s.count(anchor)==1;s=s.replace(anchor,anchor+'\n#include "pw_ps5_fixed_hint.h"')
old='    /* The kernel ignores MAP_EXCL, so claim the range with a reservation\n     * that refuses to overwrite, then map over it if needed. */\n    ptr = start;'
new='    /* A bare reservation in the system band cannot later be backed with\n     * direct memory. Refuse the guest hint before any kernel side effect,\n     * so callers can retry in the normal user area. */\n    if (!pw_ps5_fixed_hint_allowed((uintptr_t)start, size))\n    {\n        TRACE("PS5 fixed hint refused system band or overflow, base %p size %08lx\\n", start, size);\n        errno = EEXIST;\n        return MAP_FAILED;\n    }\n    /* The kernel ignores MAP_EXCL, so claim the range with a reservation\n     * that refuses to overwrite, then map over it if needed. */\n    ptr = start;'
assert s.count(old)==1;s=s.replace(old,new)
old='    /* Protecting a bare reservation does not back it with memory, so a\n     * later commit would fault on its first access: direct memory backs'
new='    if (ptr != start)\n    {\n        /* A successful NO_OVERWRITE reservation is newly owned. Release\n         * its actual returned range, never the requested unowned address. */\n        munmap(ptr, size);\n        errno = EEXIST;\n        return MAP_FAILED;\n    }\n    /* Protecting a bare reservation does not back it with memory, so a\n     * later commit would fault on its first access: direct memory backs'
assert s.count(old)==1;s=s.replace(old,new)
p.write_text(s)
header=repo/'build/counterstrike-ntdll-build/pw_ps5_fixed_hint.h'
shutil.copyfile(header,p.parent/header.name)
shutil.copyfile(header,work/'prospero/wine/ps5'/header.name)
diff=''.join(difflib.unified_diff(before.splitlines(True),s.splitlines(True),'a/dlls/ntdll/unix/virtual.c','b/dlls/ntdll/unix/virtual.c'))
(repo/'build/counterstrike-ntdll-build/ps5-fixed-hint-system-band.patch').write_text(diff)
print('Applied narrow fixed-hint range and exact-address checks')
