/* Pinned candidate publisher. Partial publication is retained on failure.
 * Caller supplies an already verified parent directory and staging fd. */
#include "candidate_publish.h"
#include "candidate_verify.h"
#include "exclusive_publish.h"
#include "pw_publication_sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct CandidateFile {const char *path;const char *sha256;} CandidateFile;
#include "candidate_files.inc"
typedef struct Directory { char path[1024]; SkyrimPublishDir handle; } Directory;
static int matches(const void *data,size_t size,const char *expected){
 uint8_t digest[32];char hex[65];const char *digits="0123456789abcdef";
 if(pw_publication_sha256_bytes(data,size,digest))return 0;
 for(size_t n=0;n<32;n++){hex[2*n]=digits[digest[n]>>4];hex[2*n+1]=digits[digest[n]&15];}hex[64]=0;return strcmp(hex,expected)==0;
}
static int read_candidate(int staging,const CandidateFile *entry,void **out,size_t *size){
 struct stat a,b;int fd=skyrim_candidate_open(staging,entry->path);uint8_t *bytes;size_t pos=0;
 if(fd<0)return -1;
 if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_uid!=geteuid()||a.st_nlink!=1||a.st_size<0||a.st_size>64*1024*1024){close(fd);errno=EPERM;return -1;}
 bytes=malloc((size_t)a.st_size+1);if(!bytes){close(fd);return -1;}
 while(pos<(size_t)a.st_size){ssize_t n=read(fd,bytes+pos,(size_t)a.st_size-pos);if(n<0&&errno==EINTR)continue;if(n<=0){free(bytes);close(fd);errno=EIO;return -1;}pos+=(size_t)n;}
 uint8_t extra;ssize_t n=read(fd,&extra,1);
 if(n!=0||fstat(fd,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||a.st_size!=b.st_size||a.st_mtime!=b.st_mtime||!matches(bytes,pos,entry->sha256)) {free(bytes);close(fd);errno=EINVAL;return -1;}
 if(close(fd)){free(bytes);return -1;}*out=bytes;*size=pos;return 0;
}
int skyrim_publish_candidate(int parent,int staging){
 SkyrimPublishRoot root={-1,-1,0,0,0};Directory dirs[64];size_t count=0;int result=-1,saved;void *bytes=NULL;
 if(skyrim_verify_candidate(staging)||skyrim_claim(parent,&root))return -1;
 for(int pass=0;pass<2;pass++)for(size_t i=0;i<sizeof(candidate_files)/sizeof(candidate_files[0]);i++){
  const CandidateFile *entry=&candidate_files[i];int is_param=!strcmp(entry->path,"sce_sys/param.json");
  if(is_param!=pass)continue;
  char path[1024],prefix[1024]={0},*part,*slash;SkyrimPublishDir *parent_dir=NULL;size_t size;
  if(strlen(entry->path)>=sizeof(path)){errno=ENAMETOOLONG;goto done;}strcpy(path,entry->path);part=path;
  while((slash=strchr(part,'/'))){*slash=0;size_t len=strlen(prefix);if(len)prefix[len++]='/';if(len+strlen(part)>=sizeof(prefix)){errno=ENAMETOOLONG;goto done;}strcpy(prefix+len,part);
   Directory *found=NULL;for(size_t j=0;j<count;j++)if(!strcmp(dirs[j].path,prefix)){found=&dirs[j];break;}
   if(!found){if(count>=64){errno=EOVERFLOW;goto done;}found=&dirs[count];strcpy(found->path,prefix);if(skyrim_child_dir(&root,parent_dir,part,&found->handle))goto done;count++;}
   parent_dir=&found->handle;part=slash+1;
  }
  if(read_candidate(staging,entry,&bytes,&size))goto done;
  int status=parent_dir?skyrim_child_file(parent_dir,part,bytes,size,0755):skyrim_create_file(&root,part,bytes,size,0755);
  free(bytes);bytes=NULL;if(status)goto done;
 }
 result=0;
done:saved=errno;free(bytes);while(count)skyrim_child_close(&dirs[--count].handle);skyrim_close(&root);errno=saved;return result;
}
