from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'shims.c'
    subprocess.run([sys.executable, str(root / 'tools/prepare_vapor_file_shims.py'), str(output)], check=True)
    text = output.read_text()
    assert text.count('int __wrap_fstatat(') == 1
    assert text.count('int __wrap_unlinkat(') == 1
    assert 'directory_call(SYS_fstatat, directory, path, (long)(uintptr_t)value, flags)' in text
    assert 'directory_call(SYS_unlinkat, directory, path, flags, 0)' in text
