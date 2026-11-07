#!/usr/bin/env python3
"""Host smoke test of the exact 120-second source configuration, no console."""
import fcntl, socket, subprocess, tempfile, time
from pathlib import Path
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temp:
    exe = Path(temp) / 'observer-120s'
    probe = socket.socket(); probe.bind(('127.0.0.1', 0)); port = probe.getsockname()[1]; probe.close()
    subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-DOBS_SECONDS=120', f'-DOBS_PORT={port}',
                    str(root/'research/skyrim/loopback_observer.c'), '-o', str(exe)], check=True)
    receiver, sender = socket.socketpair(); flags = fcntl.fcntl(sender, fcntl.F_GETFL)
    process = subprocess.Popen([str(exe)], stdout=sender)
    assert receiver.recv(128) == b'OBS ready=1\n'
    time.sleep(3)
    assert process.poll() is None, 'observer stopped before the long window'
    peer = socket.create_connection(('127.0.0.1', port)); peer.close()
    assert process.wait(timeout=4) == 0
    assert fcntl.fcntl(sender, fcntl.F_GETFL) == flags
    sender.close(); data = b''
    while True:
        part = receiver.recv(4096)
        if not part: break
        data += part
    receiver.close()
    assert b'reason=eof connections=1 bytes=0' in data
print('120-second variant: host startup, sustained wait, EOF, unchanged socket flags passed')
