#!/usr/bin/env python3
"""Host-only fake sysctl tests; does not query host or console processes."""
import os, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as td:
    d=Path(td); (d/'sys').mkdir()
    (d/'sys/sysctl.h').write_text('''#include <sys/types.h>\n#define CTL_KERN 1\n#define KERN_PROC 14\n#define KERN_PROC_PID 1\nint sysctl(const int *, unsigned, void *, size_t *, const void *, size_t);\n''')
    (d/'sys/user.h').write_text('''#include <sys/types.h>\nstruct kinfo_proc { int ki_structsize; pid_t ki_pid; uid_t ki_uid; };\n''')
    (d/'fake.c').write_text('''#include <sys/types.h>\n#include <sys/user.h>\n#include <unistd.h>\n#include <stdlib.h>\n#include <errno.h>\nint sysctl(const int *m,unsigned n,void *out,size_t *len,const void *in,size_t inlen) {\n if(n!=4||m[0]!=1||m[1]!=14||m[2]!=1||m[3]!=getpid()||in||inlen||*len!=sizeof(struct kinfo_proc)) abort();\n int mode=atoi(getenv("TEST_CASE")); if(mode==1){errno=EPERM; return -1;}\n struct kinfo_proc *p=out; p->ki_structsize=sizeof(*p); p->ki_pid=getpid(); p->ki_uid=geteuid();\n if(mode==2)*len=sizeof(*p)-1; if(mode==3)p->ki_structsize--; if(mode==4)p->ki_pid++; if(mode==5)p->ki_uid++; return 0; }\n''')
    exe=d/'probe'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(d),str(root/'research/skyrim/self_proc_sysctl_probe.c'),str(d/'fake.c'),'-o',str(exe)],check=True)
    assert subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=3).returncode != 0
    cases=[(0,b'sysctl_ok=1 errno=0 abi=1 pid_match=1 euid_match=1 valid=1',0),
           (1,b'sysctl_ok=0 errno=1 abi=0 pid_match=0 euid_match=0 valid=0',1),
           (2,b'abi=0 pid_match=0 euid_match=0 valid=0',1),
           (3,b'abi=0 pid_match=0 euid_match=0 valid=0',1),
           (4,b'abi=1 pid_match=0 euid_match=1 valid=0',1),
           (5,b'abi=1 pid_match=1 euid_match=0 valid=0',1)]
    for mode, expected, status in cases:
        reader,writer=socket.socketpair(); env=dict(os.environ, TEST_CASE=str(mode))
        p=subprocess.Popen([str(exe)],stdout=writer,env=env); writer.close()
        assert p.wait(timeout=3)==status
        msg=reader.recv(512); reader.close(); assert expected in msg, msg
print('self-only sysctl host fake: exact self MIB, no new value, errors, lengths, pid and euid passed')
