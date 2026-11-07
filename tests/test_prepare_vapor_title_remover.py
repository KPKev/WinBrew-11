from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
original = (root / 'vendor/playden-source/tools/title-remover/main.c').read_text()
with tempfile.TemporaryDirectory() as temp:
    output = Path(temp) / 'remover.c'
    subprocess.run([sys.executable, str(root / 'tools/prepare_vapor_title_remover.py'), str(output)], check=True)
    text = output.read_text()
    assert text.count('#define REQUEST_DIR "/data/wine11/wine11-games/vapor-title-remover"') == 1
    assert text.count('#define PLAYDEN_STATE "/data/wine11/wine11-games"') == 1
    assert 'if (seteuid(owner) != 0) return 1;' in text
    assert 'if (seteuid(0) != 0) return 6;' in text
    restored = text.replace('/data/wine11/wine11-games/vapor-title-remover', '/data/playden/title-remover') \
                   .replace('/data/wine11/wine11-games', '/data/playden/runtime-state') \
                   .replace('  if (seteuid(owner) != 0) return 1;\n', '') \
                   .replace('  if (seteuid(0) != 0) return 6;\n', '')
    assert restored == original
