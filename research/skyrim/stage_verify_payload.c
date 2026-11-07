/* Read-only scoped diagnostics; no grant, publication, chmod or writes. */
#include "candidate_verify.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#define STAGE "/data/wine11-skyrim-stage-12770aab2c584af6836d427446926c42"
static void report(const char *name,int fd) {
 struct stat s;
 if(fd<0||fstat(fd,&s)){printf("SKYRIM_STAGE %s=unavailable errno=%d\n",name,errno);return;}
 printf("SKYRIM_STAGE %s uid=%u gid=%u mode=%04o links=%lu type=%s\n",name,(unsigned)s.st_uid,(unsigned)s.st_gid,(unsigned)(s.st_mode&07777),(unsigned long)s.st_nlink,S_ISDIR(s.st_mode)?"dir":S_ISREG(s.st_mode)?"file":"other");
}
int main(void) {
 printf("SKYRIM_STAGE start euid=%u egid=%u\n",(unsigned)geteuid(),(unsigned)getegid());fflush(stdout);
 int parent=open("/data/homebrew",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);report("homebrew",parent);
 int root=open(STAGE,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);report("stage",root);
 int binary=root>=0?openat(root,"eboot.bin",O_RDONLY|O_NOFOLLOW):-1;report("eboot",binary);
 int result=root>=0?skyrim_verify_candidate(root):-1;
 printf("SKYRIM_STAGE candidate=%s errno=%d\n",result?"refused":"passed",result?errno:0);
 if(binary>=0)close(binary);
 if(root>=0)close(root);
 if(parent>=0)close(parent);
 puts("SKYRIM_STAGE done");fflush(stdout);return result?1:0;
}
