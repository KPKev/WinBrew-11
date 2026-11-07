#!/usr/bin/env python3
"""Host-only synthetic sysctl records; no process or console query."""
import os, socket, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp); (d/'sys').mkdir()
    (d/'sys/sysctl.h').write_text('''#include <sys/types.h>\n#define CTL_KERN 1\n#define KERN_PROC 14\n#define KERN_PROC_PID 1\nint sysctl(const int *, unsigned, void *, size_t *, const void *, size_t);\n''')
    (d/'sys/user.h').write_text('''#include <sys/types.h>\nstruct kinfo_proc { int ki_structsize; char reserved[60]; pid_t ki_pid; uid_t ki_uid; uid_t ki_ruid; gid_t ki_rgid; char rest[1008]; };\n''')
    (d/'fake.c').write_text('''#include <sys/types.h>\n#include <sys/user.h>\n#include <unistd.h>\n#include <stdlib.h>\n#include <errno.h>\n#include <string.h>\nint sysctl(const int *m,unsigned n,void *out,size_t *len,const void *in,size_t inlen) {\n if(n!=4||m[0]!=1||m[1]!=14||m[2]!=1||m[3]!=getpid()||!out||!len||*len!=4096||in||inlen) abort();\n int mode=atoi(getenv("TEST_CASE")); if(mode==1){errno=ENOMEM;return -1;}\n struct kinfo_proc *p=out; memset(out,0x5a,*len); p->ki_structsize=1096;p->ki_pid=getpid();p->ki_uid=geteuid();p->ki_ruid=getuid();p->ki_rgid=getgid();\n *len=mode==2?2:mode==3?4097:1096; if(mode==4)p->ki_structsize=1088; if(mode==5)p->ki_pid++; if(mode==6)p->ki_uid++; if(mode==7)p->ki_ruid++; if(mode==8)p->ki_rgid++;return 0;}\n''')
    exe=d/'probe'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(d),str(root/'research/skyrim/self_proc_candidate_probe.c'),str(d/'fake.c'),'-o',str(exe)],check=True)
    assert subprocess.run([str(exe)],stdout=subprocess.PIPE,timeout=3).returncode != 0
    cases=[(0,b'bounded=1 length=1096',b'size_match=1 pid_match=1 euid_match=1 ruid_match=1 rgid_match=1',0),
           (1,b'ok=0 bounded=0',b'size_match=0 pid_match=0 euid_match=0',1),
           (2,b'bounded=1 length=2',b'size_match=0 pid_match=0',1),
           (3,b'bounded=0 length=0',b'size_match=0 pid_match=0',1),
           (4,b'bounded=1 length=1096',b'size_match=0 pid_match=0',1),
           (5,b'pid_match=0',b'euid_match=1',1),(6,b'euid_match=0',b'pid_match=1',1),
           (7,b'ruid_match=0',b'pid_match=1',1),(8,b'rgid_match=0',b'pid_match=1',1)]
    for mode,a,b,status in cases:
        reader,writer=socket.socketpair(); env=dict(os.environ,TEST_CASE=str(mode))
        p=subprocess.Popen([str(exe)],stdout=writer,env=env);writer.close();assert p.wait(timeout=3)==status
        msg=reader.recv(512);reader.close();assert a in msg and b in msg and b'ZZZZ' not in msg,msg
print('candidate probe host: self MIB, bounds, record size, field mismatches and redaction passed')
