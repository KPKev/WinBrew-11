/* UNTESTED write-capable prototype; no app.db reservation. */
#include "candidate_publish.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef SKYRIM_STAGE_PATH
#define SKYRIM_STAGE_PATH "/data/wine11-skyrim-diagnostic-stage"
#endif
int main(int argc,char **argv){
 if(argc!=3||strcmp(argv[1],"--publish")||strcmp(argv[2],"--registration-unreserved")){
  puts("SKYRIM_PUBLISH refused=explicit-authorization-required");return 1;
 }
 int parent=open("/data/homebrew",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 int staging=open(SKYRIM_STAGE_PATH,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 if(parent<0||staging<0){puts("SKYRIM_PUBLISH refused=directory");if(parent>=0)close(parent);if(staging>=0)close(staging);return 1;}
 int result=skyrim_publish_candidate(parent,staging);close(staging);close(parent);
 puts(result?"SKYRIM_PUBLISH refused=inspect-partial":"SKYRIM_PUBLISH published=1 registration_reserved=0");return result?1:0;
}
