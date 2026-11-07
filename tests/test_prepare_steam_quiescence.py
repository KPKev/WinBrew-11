from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    base = Path(temp)
    subprocess.run([sys.executable, str(root / 'tools/prepare_steam_quiescence.py'), str(base / 'normal')], check=True)
    subprocess.run([sys.executable, str(root / 'tools/prepare_steam_cancel_cleanup.py'), str(base / 'experimental')], check=True)
    header = (base / 'normal/installs/install_service.h').read_text()
    source = (base / 'normal/installs/install_service.cpp').read_text()
    assert 'bool WorkerIdleForExit();' in header
    assert 'removal_active_ = true;' in source and 'removal_active_ = false;' in source
    assert 'cancel_cleanup_failures_' not in header
    assert (base / 'experimental/installs/install_service.h').read_bytes() != (base / 'normal/installs/install_service.h').read_bytes()
print('PASS: normal and experimental install service overlays remain distinct')
