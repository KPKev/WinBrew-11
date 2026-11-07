#!/usr/bin/env python3
"""Run actual preserved ps5log init/close against the isolated host observer."""
import socket, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temp:
    observer, sender = Path(temp)/'observer', Path(temp)/'sender'
    probe = socket.socket(); probe.bind(('127.0.0.1', 0)); port = probe.getsockname()[1]; probe.close()
    flags = ['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
    subprocess.run(flags+[f'-DOBS_PORT={port}', '-DOBS_SECONDS=3', str(root/'research/skyrim/loopback_bye_observer.c'), '-o', str(observer)], check=True)
    subprocess.run(flags+[str(root/'research/skyrim/ps5log_bye_integration_sender.c'), '-o', str(sender)], check=True)
    for reason in ('invalid-descriptor', 'invalid-per-game-arguments', 'per-game-bootstrap', 'wine64-exit'):
        reader, writer = socket.socketpair()
        receiver = subprocess.Popen([str(observer)], stdout=writer)
        assert reader.recv(128) == b'OBS ready=1\n'
        subprocess.run([str(sender), str(port), reason], check=True, timeout=3)
        assert receiver.wait(timeout=4) == 0
        writer.close(); data = b''
        while True:
            part = reader.recv(4096)
            if not part: break
            data += part
        reader.close()
        assert b'OBS hello=matched-unverified\n' in data, data
        assert b'OBS reason=eof connections=1 ' in data, data
        if reason == 'wine64-exit': assert b'OBS bye=' not in data, data
        else: assert data.count(('OBS bye='+reason+'\n').encode()) == 1, data
print('preserved ps5log init/close: matching HELLO, all three BYE reasons, rejected other reason, EOF passed')
