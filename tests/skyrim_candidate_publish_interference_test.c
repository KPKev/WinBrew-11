#include "candidate_publish.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
static int stage_fd,armed=1;
ssize_t __real_write(int,const void*,size_t);
ssize_t __wrap_write(int fd,const void*data,size_t size){if(armed){armed=0;int target=openat(stage_fd,"eboot.bin",O_WRONLY|O_APPEND|O_NOFOLLOW);assert(target>=0);assert(__real_write(target,"x",1)==1);close(target);}return __real_write(fd,data,size);}
int main(int argc,char **argv){assert(argc==3);int parent=open(argv[1],O_RDONLY|O_DIRECTORY);stage_fd=open(argv[2],O_RDONLY|O_DIRECTORY);assert(parent>=0&&stage_fd>=0);assert(skyrim_publish_candidate(parent,stage_fd)<0);int dest=openat(parent,"PPSA77383",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(dest>=0);assert(faccessat(dest,"sce_sys/param.json",F_OK,0)<0&&errno==ENOENT);close(dest);close(parent);close(stage_fd);return 0;}
