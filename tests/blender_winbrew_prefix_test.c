#define _GNU_SOURCE
#include "../runtime-patches/blender/launcher/pw_winbrew_group.h"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
int main(void){char root[]="/tmp/winbrew-prefix-XXXXXX",p[1024],q[1024],text[256];assert(mkdtemp(root));const char *dirs[]={"drive_c","drive_c/users","drive_c/users/prospero","drive_c/users/prospero/Documents","drive_c/users/prospero/Desktop"};for(unsigned i=0;i<5;i++){snprintf(p,sizeof p,"%s/%s",root,dirs[i]);assert(!mkdir(p,0700));}snprintf(p,sizeof p,"%s/drive_c/users/prospero/Documents/original.blend",root);int fd=open(p,O_CREAT|O_EXCL|O_WRONLY,0600);assert(fd>=0&&write(fd,"preserve",8)==8);close(fd);
assert(!pw_winbrew_prefix_connect(root));assert(!pw_winbrew_prefix_connect(root));snprintf(p,sizeof p,"%s/drive_c/users/prospero/Documents.before-WinBrew/original.blend",root);fd=open(p,O_RDONLY);assert(fd>=0&&read(fd,text,8)==8&&!memcmp(text,"preserve",8));close(fd);
snprintf(q,sizeof q,"%s/drive_c/users/prospero/.pw-symlinks",root);fd=open(q,O_RDONLY);assert(fd>=0);ssize_t n=read(fd,text,sizeof text-1);assert(n>0);text[n]=0;assert(strstr(text,"Documents\t/data/WinBrew/Users/Default/Documents\n"));close(fd);
fd=open(q,O_WRONLY|O_TRUNC);assert(fd>=0&&write(fd,"foreign",7)==7);close(fd);assert(pw_winbrew_prefix_connect(root)<0&&errno==EEXIST);
assert(!unlink(q));assert(!unlink(p));snprintf(p,sizeof p,"%s/drive_c/users/prospero/Documents.before-WinBrew",root);assert(!rmdir(p));snprintf(p,sizeof p,"%s/drive_c/users/prospero/Desktop.before-WinBrew",root);assert(!rmdir(p));for(int i=2;i>=0;i--){snprintf(p,sizeof p,"%s/%s",root,dirs[i]);assert(!rmdir(p));}assert(!rmdir(root));puts("PASS: prefix migration preserves files, is idempotent, and refuses foreign tables");}
