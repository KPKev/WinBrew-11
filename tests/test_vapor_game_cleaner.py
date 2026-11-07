#!/usr/bin/env python3
"""Exercise the payload against disposable root-sealed fixtures, never real games."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

if os.geteuid() != 0:
    raise SystemExit("Run this isolated fixture test as root")
binary = Path(__file__).resolve().parents[1] / "build/vapor-game-cleaner-host"
operation = "47fc159f-096c-4399-a934-4455637be846"


def fixture():
    root = Path(tempfile.mkdtemp(prefix="vapor-cleaner-test-", dir="/tmp"))
    root.chmod(0o755)
    data, home, apps = root / "wine11", root / "homebrew", root / "app"
    for path in (data, home, apps):
        path.mkdir(mode=0o755)
    games, sealed = data / "wine11-games", data / ".vapor-uninstall"
    games.mkdir(mode=0o700); os.chown(games, 1, -1)
    sealed.mkdir(mode=0o700)
    target = sealed / operation
    target.mkdir(mode=0o700); os.chown(target, 1, -1)
    nested = target / "nested"
    nested.mkdir(mode=0o700); os.chown(nested, 1, -1)
    (nested / "file").write_text("fixture")
    os.chown(nested / "file", 1, -1)
    sentinel = root / "outside"
    sentinel.write_text("untouched")
    os.symlink(sentinel, nested / "link"); os.lchown(nested / "link", 1, -1)
    other = games / "730"
    other.mkdir(mode=0o700); os.chown(other, 1, -1)
    registry = games / ".playden-games"
    registry.write_text("730\tinternal\t0\tpaused\n"); os.chown(registry, 1, -1); registry.chmod(0o666)
    st = target.stat()
    request = f"1 500 {operation} {st.st_dev} {st.st_ino} FAKE78999\n"
    req = games / ".vapor-cleanup-request.v1"
    req.write_text(request); os.chown(req, 1, -1); req.chmod(0o600)
    return root, games, target, sentinel, request


def run(root):
    return subprocess.run([str(binary), str(root)], check=False).returncode


def test_success():
    root, games, target, sentinel, request = fixture()
    try:
        assert run(root) == 0
        assert not target.exists()
        assert sentinel.read_text() == "untouched"
        assert (games / "730").is_dir()
        assert (games / ".playden-games").read_text() == "730\tinternal\t0\tpaused\n"
        assert (games / ".vapor-cleanup-request.v1").read_text() == request
        assert (games / ".vapor-cleanup-result.v1").read_text() == request + "ok\n"
        assert run(root) != 0
    finally:
        shutil.rmtree(root)


def test_refusals():
    for mutation in ("inode", "source", "registry", "title", "foreign", "malformed", "overflow", "result"):
        root, games, target, sentinel, request = fixture()
        try:
            req = games / ".vapor-cleanup-request.v1"
            if mutation == "inode": req.write_text(request.replace(f" {target.stat().st_ino} ", " 1 "))
            elif mutation == "source": (games / "500").mkdir()
            elif mutation == "registry": (games / ".playden-games").write_text("500\tinternal\t42\n")
            elif mutation == "title": (root / "homebrew" / "FAKE78999").mkdir()
            elif mutation == "foreign": (target / "nested" / "foreign").write_text("root owned")
            elif mutation == "malformed": req.write_text(request + "extra\n")
            elif mutation == "overflow": req.write_text(request.replace("1 500 ", "1 4294967296500 "))
            elif mutation == "result": (games / ".vapor-cleanup-result.v1").write_text("old")
            assert run(root) != 0, mutation
            assert target.is_dir(), mutation
            assert sentinel.read_text() == "untouched", mutation
            assert (games / "730").is_dir(), mutation
            if mutation != "result": assert not (games / ".vapor-cleanup-result.v1").exists(), mutation
        finally:
            shutil.rmtree(root)


test_success()
test_refusals()
print("PASS: sealed cleaner validates identity, refuses conflicts and preserves outside files")
