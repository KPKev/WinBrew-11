/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "pw_winbrew_group.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
/* The privileged helper assigns the group before Wine initialization.
 * Do not invoke title group syscalls: they are not supported by this runtime. */
int pw_winbrew_group_enter(void) {
 const char *paths[]={"/data/WinBrew/Users/Default/Documents","/data/WinBrew/Users/Default/Desktop"};
 for(unsigned i=0;i<2;i++) {struct stat st;
  if(lstat(paths[i],&st)||!S_ISDIR(st.st_mode)||st.st_uid!=0||st.st_gid!=365670||(st.st_mode&07777)!=0770||access(paths[i],R_OK|W_OK|X_OK)) {errno=EACCES;return -1;}
 }
 return 0;
}

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
/* Wine on this console represents symlinks with .pw-symlinks. Preserve the
 * old private folders under explicit backup names before connecting the defaults. */
int pw_winbrew_prefix_connect(const char *prefix) {
 static const char table[]="Documents\t/data/WinBrew/Users/Default/Documents\nDesktop\t/data/WinBrew/Users/Default/Desktop\n";
 char root[1024],target[1100],from[2][1100],backup[2][1100];
 const char *names[]={"Documents","Desktop"};int moved[2]={0},fd=-1,created=0,error=0;struct stat st;
 if(!prefix||snprintf(root,sizeof root,"%s/drive_c/users/prospero",prefix)>=(int)sizeof root){errno=ENAMETOOLONG;return -1;}
 if(lstat(root,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()){errno=EACCES;return -1;}
 snprintf(target,sizeof target,"%s/.pw-symlinks",root);
 fd=open(target,O_RDONLY|O_NOFOLLOW);
 if(fd>=0){char data[sizeof table];ssize_t n=read(fd,data,sizeof data);int valid=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_uid==geteuid()&&n==(ssize_t)sizeof(table)-1&&!memcmp(data,table,sizeof(table)-1);close(fd);if(!valid)errno=EEXIST;return valid?0:-1;}
 if(errno!=ENOENT)return -1;
 for(int i=0;i<2;i++){
  snprintf(from[i],sizeof from[i],"%s/%s",root,names[i]);snprintf(backup[i],sizeof backup[i],"%s/%s.before-WinBrew",root,names[i]);
  if(!lstat(backup[i],&st)||errno!=ENOENT){error=EEXIST;goto fail;}
  if(!lstat(from[i],&st)){if(!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()){error=EPERM;goto fail;}if(rename(from[i],backup[i])){error=errno;goto fail;}moved[i]=1;}else if(errno!=ENOENT){error=errno;goto fail;}
 }
 fd=open(target,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0){error=errno;goto fail;}created=1;
 size_t at=0;while(at<sizeof(table)-1){ssize_t n=write(fd,table+at,sizeof(table)-1-at);if(n<=0){error=errno?errno:EIO;goto fail;}at+=(size_t)n;}
 if(fsync(fd)){error=errno;goto fail;}close(fd);return 0;
fail:
 if(fd>=0)close(fd);
 if(created)unlink(target);
 for(int i=1;i>=0;i--)if(moved[i]&&rename(backup[i],from[i]))error=EIO;
 errno=error;return -1;
}
