#!/usr/bin/env python3
"""Host-only mocked sysctl; no process or console querying."""
import os, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
    d=Path(td); (d/'sys').mkdir()
    (d/'sys/sysctl.h').write_text('''#include <sys/types.h>\n#define CTL_KERN 1\n#define KERN_PROC 14\n#define KERN_PROC_PID 1\nint sysctl(const int *, unsigned, void *, size_t *, const void *, size_t);\n''')
    (d/'sys/user.h').write_text('''struct kinfo_proc { char bytes[1088]; };\n''')
    (d/'fake.c').write_text('''#include <sys/types.h>\n#include <unistd.h>\n#include <stdlib.h>\n#include <errno.h>\nint sysctl(const int *m,unsigned n,void *out,size_t *len,const void *in,size_t inlen) {\n if(n!=4||m[0]!=1||m[1]!=14||m[2]!=1||m[3]!=getpid()||out||!len||*len!=0||in||inlen) abort();\n int mode=atoi(getenv("TEST_CASE")); if(mode==1){errno=ENOMEM; *len=999; return -1;}\n *len=mode==2?0:mode==3?65537:mode==4?65536:1088; return 0; }\n''')
    exe=d/'probe'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(d),str(root/'research/skyrim/self_proc_size_probe.c'),str(d/'fake.c'),'-o',str(exe)],check=True)
    assert subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=3).returncode != 0
    cases=[(0,b'ok=1 errno=0 in_range=1 size=1088 expected=1088',0),
           (1,b'ok=0 errno=12 in_range=0 size=0 expected=1088',1),
           (2,b'ok=1 errno=0 in_range=0 size=0 expected=1088',1),
           (3,b'ok=1 errno=0 in_range=0 size=0 expected=1088',1),
           (4,b'ok=1 errno=0 in_range=1 size=65536 expected=1088',0)]
    for mode,expected,status in cases:
        reader,writer=socket.socketpair(); env=dict(os.environ,TEST_CASE=str(mode))
        p=subprocess.Popen([str(exe)],stdout=writer,env=env); writer.close()
        assert p.wait(timeout=3)==status
        msg=reader.recv(512); reader.close(); assert expected in msg, msg
print('self size query: null buffers, exact self MIB, error, zero and cap passed')
