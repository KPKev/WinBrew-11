#include "candidate_publish.h"
#include "candidate_verify.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc,char**argv){assert(argc==3);int parent=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);int staging=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(parent>=0&&staging>=0);assert(skyrim_publish_candidate(parent,staging)==0);int dest=openat(parent,"PPSA77383",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(dest>=0);assert(skyrim_verify_candidate(dest)==0);assert(skyrim_publish_candidate(parent,staging)<0&&errno==EEXIST);close(dest);close(staging);close(parent);puts("PASS: pinned candidate published without overwrite; resulting file hashes verified");return 0;}
