"""Generate an isolated request namespace for Playden's pinned remover source.

This creates source only. It does not authorize or execute a removal.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / 'vendor/playden-source/tools/title-remover/main.c').read_text(encoding='utf-8')
for old, new in (
    ('#define REQUEST_DIR "/data/playden/title-remover"',
     '#define REQUEST_DIR "/data/wine11/wine11-games/vapor-title-remover"'),
    ('#define PLAYDEN_STATE "/data/playden/runtime-state"',
     '#define PLAYDEN_STATE "/data/wine11/wine11-games"'),
    # The payload starts as root, but PS5 UID 0 cannot traverse the private
    # UID 1 games directory. Read the owner-protected request as that owner.
    ('  const uid_t owner = playden.st_uid;\n',
     '  const uid_t owner = playden.st_uid;\n  if (seteuid(owner) != 0) return 1;\n'),
    # Restore the loader's real root identity before the native uninstall API.
    ('  unsigned code = (unsigned)sceAppInstUtilInitialize();',
     '  if (seteuid(0) != 0) return 6;\n  unsigned code = (unsigned)sceAppInstUtilInitialize();'),
):
    if source.count(old) != 1:
        raise SystemExit('Pinned title remover changed; refusing an ambiguous transformation')
    source = source.replace(old, new, 1)
if len(sys.argv) != 2:
    raise SystemExit('usage: prepare_vapor_title_remover.py OUTPUT_C')
Path(sys.argv[1]).write_text(source, encoding='utf-8', newline='\n')
