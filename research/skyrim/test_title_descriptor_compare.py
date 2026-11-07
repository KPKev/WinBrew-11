#!/usr/bin/env python3
import socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp); exe=d/'observer'; titles=['FAKE78003','FAKE78004','FAKE78005']
    for t in titles: (d/t).mkdir()
    quake=d/titles[0]/'game-launch.json'; quake.write_text('SECRET-QUAKE'); quake.chmod(0o600)
    blackjack=d/titles[1]/'game-launch.json'; blackjack.symlink_to(d/'missing')
    # Skyrim descriptor missing, proving an inaccessible/absent path remains insufficient.
    fake=d/'wrap.c'; fake.write_text('''#define _DEFAULT_SOURCE 1\n#include <sys/types.h>\n#include <sys/stat.h>\n#include <stdlib.h>\n#include <string.h>\nint __real_lstat(const char*,struct stat*);\nint __wrap_lstat(const char *p,struct stat *s){static const char *allowed[]={ROOT "/FAKE78003",ROOT "/FAKE78003/game-launch.json",ROOT "/FAKE78004",ROOT "/FAKE78004/game-launch.json",ROOT "/FAKE78005",ROOT "/FAKE78005/game-launch.json"}; for(unsigned i=0;i<6;i++)if(!strcmp(p,allowed[i]))return __real_lstat(p,s);abort();}\n''')
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DOBS_TEST_FIXTURES',
                    '-DOBS_TEST_ROOT="'+str(d)+'"','-DROOT="'+str(d)+'"',
                    str(root/'research/skyrim/title_descriptor_compare.c'),str(fake),'-Wl,--wrap=lstat','-o',str(exe)],check=True)
    assert subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=3).returncode != 0
    before=quake.stat(); reader,writer=socket.socketpair()
    p=subprocess.Popen([str(exe)],stdout=writer); writer.close(); assert p.wait(timeout=3)==0
    output=b''
    while True:
        part=reader.recv(4096)
        if not part: break
        output+=part
    reader.close(); text=output.decode(); after=quake.stat()
    assert len(text.splitlines())==6 and str(d) not in text and 'SECRET' not in text
    assert 'title=FAKE78003 item=descriptor first=1 second=1 type=regular' in text
    assert 'mode=0600' in text and 'title=FAKE78004 item=descriptor first=1 second=1 type=symlink' in text
    assert 'title=FAKE78005 item=descriptor first=0 second=0 stable=0' in text
    assert (before.st_ino,before.st_mtime_ns,before.st_mode)==(after.st_ino,after.st_mtime_ns,after.st_mode)
    (d/titles[2]).rmdir(); (d/titles[2]).symlink_to(d/'missing')
    reader,writer=socket.socketpair(); p=subprocess.Popen([str(exe)],stdout=writer);writer.close();assert p.wait(timeout=3)==0
    text=reader.recv(4096).decode();reader.close()
    assert 'title=FAKE78005 item=parent first=1 second=1 type=symlink' in text
    assert 'title=FAKE78005 item=descriptor first=0 second=0 stable=0' in text
print('fixed title metadata: allowed-path interception, missing, symlinks, redaction and socket guard passed')
