#!/usr/bin/env python3
import os, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=root/'research/skyrim/descriptor_stat_observer.c'
with tempfile.TemporaryDirectory() as tmp:
    base=Path(tmp); title=base/'title'; desc=title/'game-launch.json'; title.mkdir()
    exe=base/'observer'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-DOBS_TITLE_ROOT="'+str(title)+'"','-DOBS_DESCRIPTOR="'+str(desc)+'"',str(src),'-o',str(exe)],check=True)
    def run():
        reader, writer=socket.socketpair()
        p=subprocess.Popen([str(exe)],stdout=writer); writer.close()
        assert p.wait(timeout=3)==0
        output=b''
        while True:
            part=reader.recv(1024)
            if not part: break
            output+=part
        reader.close()
        assert str(base).encode() not in output
        return output.decode()
    assert subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=3).returncode != 0
    desc.write_text('SECRET-DESCRIPTOR-CONTENT'); desc.chmod(0o600)
    before=desc.stat(); text=run(); after=desc.stat()
    assert 'item=parent first=1 second=1 type=directory' in text
    assert 'item=descriptor first=1 second=1 type=regular' in text and 'mode=0600' in text
    assert text.count('stable=1') == 2 and 'SECRET' not in text
    assert before.st_ino == after.st_ino and before.st_mtime_ns == after.st_mtime_ns and before.st_mode == after.st_mode
    desc.unlink(); desc.symlink_to(base/'nonexistent')
    text=run(); assert 'item=descriptor first=1 second=1 type=symlink' in text and 'stable=1' in text
    desc.unlink(); text=run(); assert 'item=descriptor first=0 second=0 stable=0' in text
    title.rmdir(); title.symlink_to(base/'nonexistent')
    text=run(); assert 'item=parent first=1 second=1 type=symlink' in text
    assert 'item=descriptor first=0 second=0 stable=0' in text
print('descriptor stat: normal, missing, symlink, parent refusal and socket guard passed')
