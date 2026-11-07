/* Stage-bound single-purpose publisher. Registration is not reserved. */
#include "candidate_publish.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void){
 puts("SKYRIM_PUBLISH start registration_reserved=0");fflush(stdout);
 if(geteuid()!=0){puts("SKYRIM_PUBLISH refused=uid");fflush(stdout);return 1;}
 int parent=open("/data/homebrew",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 int staging=open("/data/wine11-skyrim-stage-12770aab2c584af6836d427446926c42",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 struct stat p,s;
 if(parent<0||staging<0||fstat(parent,&p)||fstat(staging,&s)||p.st_uid!=0||s.st_uid!=0){
  puts("SKYRIM_PUBLISH refused=directory-or-owner");if(parent>=0)close(parent);if(staging>=0)close(staging);fflush(stdout);return 1;
 }
 int result=skyrim_publish_candidate(parent,staging);int saved=errno;close(staging);close(parent);
 if(result)printf("SKYRIM_PUBLISH refused=inspect-partial errno=%d\n",saved);
 else puts("SKYRIM_PUBLISH published=1 registration_reserved=0");
 fflush(stdout);return result?1:0;
}
