/* SPDX-License-Identifier: GPL-3.0-or-later
 * Read-only installed-title ownership/link audit; no file writes or game
 * launches. Private markers are read under their owner and not printed. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
static char paths[2048][1024];
int main(void){
 DIR* home=opendir("/data/homebrew");if(!home){puts("RUNTIME_OWNER_AUDIT fail=home");return 1;}
 puts("RUNTIME_OWNER_AUDIT start read_only=1");struct dirent* entry;
 while((entry=readdir(home))){
  if(strncmp(entry->d_name,"FAKE78",6)||strlen(entry->d_name)!=9)continue;
  char root[1024],markerpath[1024];snprintf(root,sizeof(root),"/data/homebrew/%s",entry->d_name);
  int length=snprintf(markerpath,sizeof(markerpath),"%s/.playden-title-owner",root);
  if(length<0||(unsigned)length>=sizeof(markerpath))return 1;
  struct stat title,marker;if(lstat(root,&title)||!S_ISDIR(title.st_mode)||lstat(markerpath,&marker)||!S_ISREG(marker.st_mode))continue;
  char text[4096]={0};if(seteuid(marker.st_uid))return 1;
  FILE* file=fopen(markerpath,"rb");if(file){size_t n=fread(text,1,sizeof(text)-1,file);text[n]=0;fclose(file);}
  if(seteuid(0))return 1;
  int vapor=strstr(text,"install_id=wine11-steam-")!=0;
  unsigned count=1,files=0,foreign=0,wine_foreign=0,unwritable=0,links=0,errors=0;
  strcpy(paths[0],root);
  for(unsigned i=0;i<count;i++){
   struct stat parent;if(lstat(paths[i],&parent)||!S_ISDIR(parent.st_mode)){errors++;continue;}
   if(parent.st_uid!=title.st_uid){foreign++;if(strstr(paths[i],"/win/wine"))wine_foreign++;}
   if(!(parent.st_mode&0200))unwritable++;
   DIR* d=opendir(paths[i]);if(!d){errors++;continue;}struct dirent* item;
   while((item=readdir(d))){
    if(!strcmp(item->d_name,".")||!strcmp(item->d_name,".."))continue;
    char path[1024];length=snprintf(path,sizeof(path),"%s/%s",paths[i],item->d_name);struct stat s;
    if(length<0||(unsigned)length>=sizeof(path)||lstat(path,&s)){errors++;continue;}
    if(S_ISDIR(s.st_mode)){
     if(count>=2048){closedir(d);closedir(home);puts("RUNTIME_OWNER_AUDIT fail=bound");return 1;}
     strcpy(paths[count++],path);
    }else if(S_ISLNK(s.st_mode))links++;
    else if(S_ISREG(s.st_mode))files++;
    else errors++;
   }closedir(d);
  }
  printf("RUNTIME_OWNER_AUDIT title=%s vapor=%d owner=%u directories=%u files=%u foreign_directories=%u foreign_wine=%u unwritable=%u symlinks=%u errors=%u\n",entry->d_name,vapor,(unsigned)title.st_uid,count,files,foreign,wine_foreign,unwritable,links,errors);fflush(stdout);
 }
 closedir(home);puts("RUNTIME_OWNER_AUDIT done file_writes=0");fflush(stdout);return 0;
}
