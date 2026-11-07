#define _GNU_SOURCE
#include "candidate_verify.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
int main(int argc,char**argv){assert(argc==3);int fd=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(fd>=0);int result=skyrim_verify_candidate(fd);close(fd);return (!!result)==(argv[2][0]=='1')?0:1;}
