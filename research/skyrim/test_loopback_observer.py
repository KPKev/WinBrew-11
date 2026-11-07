#!/usr/bin/env python3
import socket, subprocess, tempfile, time, fcntl
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=root/'research/skyrim/loopback_observer.c'
with tempfile.TemporaryDirectory() as td:
    exe=Path(td)/'observer'
    probe=socket.socket(); probe.bind(('127.0.0.1',0)); port=probe.getsockname()[1]; probe.close()
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',f'-DOBS_PORT={port}','-DOBS_SECONDS=2','-DOBS_MAX_BYTES=16384',str(src),'-o',str(exe)],check=True)
    # A pipe is refused before binding, without changing its flags.
    bad=subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=4)
    assert bad.returncode != 0 and bad.stdout == b''
    def start(binary=exe):
        reader, writer = socket.socketpair()
        writer.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
        flags = fcntl.fcntl(writer.fileno(), fcntl.F_GETFL)
        p=subprocess.Popen([str(binary)],stdout=writer)
        assert reader.recv(128)==b'OBS ready=1\n'
        return p, reader, writer, flags
    def finish(p, reader, writer, flags):
        p.wait(timeout=4)
        assert fcntl.fcntl(writer.fileno(), fcntl.F_GETFL) == flags
        writer.close()
        parts=[]
        while True:
            data=reader.recv(4096)
            if not data: break
            parts.append(data)
        reader.close()
        return b''.join(parts).decode()
    p, reader, writer, flags=start(); s=socket.create_connection(('127.0.0.1',port))
    s.sendall(b'HELLO ps5log/1 title=FAKE78005 app=prospero-win-wine64 boot=0x1\n1\t2\tINFO\tPW_STARTUP checkpoint=data-')
    s.sendall(b'return result=0 helper_error=1\n')
    s.sendall(b'2\t3\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1 SECRET\n' + b'x'*1100 + b'\n')
    s.sendall(b'3\t4\tINFO\tPW_STARTUP checkpoint=../../secret result=0 helper_error=1\n')
    s.close(); output=finish(p, reader, writer, flags)
    assert 'checkpoint=data-return result=0 helper_error=1' in output
    assert 'SECRET' not in output and '../../' not in output and 'x'*50 not in output
    assert p.returncode == 0, output
    p, reader, writer, flags=start(); t=time.monotonic(); output=finish(p, reader, writer, flags)
    assert 1.5 <= time.monotonic()-t < 4 and 'reason=timeout connections=0 bytes=0' in output
    p, reader, writer, flags=start(); s=socket.create_connection(('127.0.0.1',port)); s.sendall(b'X'*16384); output=finish(p, reader, writer, flags); s.close()
    assert 'reason=byte-limit connections=1 bytes=16384' in output and p.returncode == 0
    p, reader, writer, flags=start(); s=socket.create_connection(('127.0.0.1',port)); s.close()
    output=finish(p, reader, writer, flags)
    assert 'reason=eof connections=1 bytes=0' in output and p.returncode == 0
    failing=Path(td)/'wait-error'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',f'-DOBS_PORT={port}','-DOBS_SECONDS=2','-DOBS_TEST_SELECT_ERROR',str(src),'-o',str(failing)],check=True)
    p, reader, writer, flags=start(failing); output=finish(p, reader, writer, flags)
    assert 'reason=wait-error connections=0 bytes=0' in output and p.returncode != 0
    p, reader, writer, flags=start(); s=socket.create_connection(('127.0.0.1',port)); s.sendall(b'HELLO ps5log/1 title=FAKE78004 app=prospero-win-wine64 boot=1\n1\t2\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1\n'); s.close(); output=finish(p, reader, writer, flags)
    assert 'checkpoint=' not in output and 'hello=matched-unverified' not in output
    p, reader, writer, flags=start(); s=socket.create_connection(('127.0.0.1',port))
    s.sendall(b'HELLO ps5log/1 title=FAKE78005 app=prospero-win-wine64 spoof=1\n'
              b'18446744073709551616\t2\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1\n'
              b'1\t18446744073709551616\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1\n'
              b'1\t2\tINFO\tPW_STARTUP checkpoint=data-return result=9999999999999999999999999999999999 helper_error=1\n'
              b'1\t2\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=9999999999999999999999999999\n'
              b'1\t2\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1\n')
    s.close(); output=finish(p, reader, writer, flags)
    assert 'hello=matched-unverified' in output and output.count('checkpoint=') == 1
    # A forged matching HELLO is intentionally accepted; this is not authentication.
    p, reader, writer, flags=start()
    s=socket.create_connection(('127.0.0.1',port))
    s.sendall(b'HELLO ps5log/1 title=FAKE78005 app=prospero-win-wine64 spoof=1\n' +
              b'1\t2\tINFO\tPW_STARTUP checkpoint=data-return result=0 helper_error=1\n'*220)
    s.close(); p.wait(timeout=4)  # Do not drain stdout until after termination.
    assert p.returncode == 0
    backlog_output = finish(p, reader, writer, flags)
    assert backlog_output.count("checkpoint=") < 220
print('loopback observer: parsing, bounds, EOF, timeout, injected wait error and backpressure passed')
