/* Offline-reviewed package verifier. No publication, chmod, or state writes. */
#include "candidate_verify.h"
#include "pw_publication_sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct CandidateFile {const char *path;const char *sha256;} CandidateFile;
#include "candidate_files.inc"
int skyrim_candidate_open(int root,const char *path) {
 char copy[1024],*cursor,*slash;int fd=dup(root),next;
 if(fd<0||strlen(path)>=sizeof(copy)){if(fd>=0)close(fd);errno=EINVAL;return -1;}
 strcpy(copy,path);cursor=copy;
 while((slash=strchr(cursor,'/'))){*slash=0;if(!*cursor||!strcmp(cursor,".")||!strcmp(cursor,"..")){close(fd);errno=EINVAL;return -1;}
  next=openat(fd,cursor,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(fd);if(next<0)return -1;fd=next;cursor=slash+1;
 }
 if(!*cursor||!strcmp(cursor,".")||!strcmp(cursor,"..")){close(fd);errno=EINVAL;return -1;}
 next=openat(fd,cursor,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);close(fd);return next;
}
int skyrim_verify_candidate(int root) {
 struct stat root_stat,before,after;uint8_t raw[32],buf[16384];char hex[65];static const char digits[]="0123456789abcdef";
 if(fstat(root,&root_stat)||!S_ISDIR(root_stat.st_mode)||root_stat.st_uid!=geteuid()){errno=EPERM;return -1;}
 for(size_t i=0;i<sizeof(candidate_files)/sizeof(candidate_files[0]);i++){
  int fd=skyrim_candidate_open(root,candidate_files[i].path);if(fd<0)return -1;
  if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_uid!=root_stat.st_uid||before.st_nlink!=1||before.st_size<0||before.st_size>64*1024*1024){close(fd);errno=EPERM;return -1;}
  PwPublicationSha256 hash;if(pw_publication_sha256_init(&hash)){close(fd);return -1;}
  for(;;){ssize_t n=read(fd,buf,sizeof(buf));if(n<0&&errno==EINTR)continue;if(n<0){pw_publication_sha256_clear(&hash);close(fd);return -1;}if(!n)break;if(pw_publication_sha256_update(&hash,buf,(size_t)n)){close(fd);return -1;}}
  if(fstat(fd,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size||before.st_mtime!=after.st_mtime||pw_publication_sha256_finish(&hash,raw)){close(fd);errno=ESTALE;return -1;}
  if(close(fd))return -1;
  for(size_t n=0;n<32;n++){hex[n*2]=digits[raw[n]>>4];hex[n*2+1]=digits[raw[n]&15];}hex[64]=0;
  if(strcmp(hex,candidate_files[i].sha256)){errno=EINVAL;return -1;}
 }
 return 0;
}
