#include "candidate_verify.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
int main(void) {
 int root=open("/data/wine11-skyrim-diagnostic-stage",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
 if(root<0){puts("SKYRIM_CANDIDATE verification=unavailable");return 1;}
 int result=skyrim_verify_candidate(root);close(root);
 puts(result?"SKYRIM_CANDIDATE verification=refused":"SKYRIM_CANDIDATE verification=passed");return result?1:0;
}
