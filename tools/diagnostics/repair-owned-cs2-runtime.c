/* SPDX-License-Identifier: GPL-3.0-or-later
 * Provenance for the owner's one-time legacy repair. Default is plan-only.
 * Bound to FAKE78009, its UID1 owner marker, and Steam app730. Only Wine
 * directory ownership can change; file contents and modes are preserved. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#ifndef CS2_OWNER_APPLY
#define CS2_OWNER_APPLY 0
#endif
#define TITLE "/data/homebrew/FAKE78009"
#define ROOT TITLE "/win/wine"
static char dirs[256][1024];
static struct stat before[256];
static unsigned count=0,files=0;
static int fail(const char* stage){printf("CS2_OWNER_REPAIR fail=%s errno=%d\n",stage,errno);fflush(stdout);return 1;}
int main(void){
 struct stat title,marker,root;char text[4096];
 printf("CS2_OWNER_REPAIR start apply=%d\n",CS2_OWNER_APPLY);fflush(stdout);
 if(geteuid()!=0||lstat(TITLE,&title)||!S_ISDIR(title.st_mode)||title.st_uid!=1||
    lstat(TITLE "/.playden-title-owner",&marker)||!S_ISREG(marker.st_mode)||marker.st_uid!=1||marker.st_nlink!=1||
    (marker.st_mode&07777)!=0600||lstat(ROOT,&root)||!S_ISDIR(root.st_mode))return fail("identity");
 if(seteuid(1))return fail("read-as-owner");
 FILE* f=fopen(TITLE "/.playden-title-owner","rb");size_t n=f?fread(text,1,sizeof(text)-1,f):0;
 if(f)fclose(f);text[n]=0;
 int bound=strstr(text,"install_id=wine11-steam-730\n")&&strstr(text,"title_id=FAKE78009\n")&&strstr(text,"kind=title\n");
 if(seteuid(0))return fail("restore-root");if(!bound)return fail("marker-binding");
 strcpy(dirs[count],ROOT);before[count++]=root;
 for(unsigned i=0;i<count;i++){
  struct stat parent;if(lstat(dirs[i],&parent)||!S_ISDIR(parent.st_mode)||
    (parent.st_uid!=0&&parent.st_uid!=1)||parent.st_gid!=0||
    ((parent.st_mode&07777)!=0755&&(parent.st_mode&07777)!=0777))return fail("directory-policy");
  DIR* d=opendir(dirs[i]);if(!d)return fail("read-directory");
  struct dirent* e;while((e=readdir(d))){
   if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
   char path[1024];int length=snprintf(path,sizeof(path),"%s/%s",dirs[i],e->d_name);struct stat s;
   if(length<0||(unsigned)length>=sizeof(path)||lstat(path,&s)){closedir(d);return fail("child-stat");}
   if(S_ISDIR(s.st_mode)){
    if(count>=256){closedir(d);return fail("directory-bound");}
    strcpy(dirs[count],path);before[count++]=s;
   }else if(!S_ISREG(s.st_mode)||++files>10000){closedir(d);return fail("unexpected-entry");}
  }closedir(d);
 }
 unsigned changed=0;
 for(unsigned i=0;i<count;i++){
  struct stat s;if(lstat(dirs[i],&s)||!S_ISDIR(s.st_mode)||s.st_dev!=before[i].st_dev||s.st_ino!=before[i].st_ino||
   s.st_uid!=before[i].st_uid||s.st_gid!=before[i].st_gid||s.st_mode!=before[i].st_mode)return fail("identity-changed");
  if(s.st_uid==0){
   if(CS2_OWNER_APPLY&&chown(dirs[i],1,(gid_t)-1))return fail("chown-directory");
   changed++;
   if(CS2_OWNER_APPLY&&(lstat(dirs[i],&s)||s.st_uid!=1||s.st_dev!=before[i].st_dev||s.st_ino!=before[i].st_ino||
      s.st_gid!=before[i].st_gid||s.st_mode!=before[i].st_mode))return fail("verify-directory");
  }
 }
 printf("CS2_OWNER_REPAIR done apply=%d directories=%u files_untouched=%u owner_changes=%u file_writes=0\n",CS2_OWNER_APPLY,count,files,changed);fflush(stdout);return 0;
}
