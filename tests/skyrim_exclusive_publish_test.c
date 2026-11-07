#include "exclusive_publish.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
 char temp[]="/tmp/skyrim-publish-test-XXXXXX";assert(mkdtemp(temp));int parent=open(temp,O_RDONLY|O_DIRECTORY);assert(parent>=0);
 SkyrimPublishRoot r;assert(mkdirat(parent,"PPSA77383",0700)==0);assert(skyrim_claim(parent,&r)<0&&errno==EEXIST);assert(unlinkat(parent,"PPSA77383",AT_REMOVEDIR)==0);
 assert(symlinkat("/tmp",parent,"PPSA77383")==0);assert(skyrim_claim(parent,&r)<0&&errno==EEXIST);assert(unlinkat(parent,"PPSA77383",0)==0);
 assert(skyrim_claim(parent,&r)==0);assert(skyrim_root_current(&r)==0);
 assert(skyrim_create_file(&r,"one","first",5,0644)==0);assert(skyrim_create_file(&r,"one","other",5,0644)<0&&errno==EEXIST);
 int f=openat(r.root_fd,"one",O_RDONLY|O_NOFOLLOW);assert(f>=0);char b[8]={0};assert(read(f,b,8)==5&&!memcmp(b,"first",5));close(f);
 assert(symlinkat("one",r.root_fd,"link")==0);assert(skyrim_create_file(&r,"link","x",1,0644)<0&&errno==EEXIST);
 assert(skyrim_create_file(&r,"../escape","x",1,0644)<0);
 int sub=-1;assert(skyrim_make_dir(&r,"sub",&sub)==0);close(sub);assert(skyrim_make_dir(&r,"sub",&sub)<0&&errno==EEXIST);
 SkyrimPublishDir d1,d2;assert(skyrim_child_dir(&r,NULL,"deep",&d1)==0);assert(skyrim_child_dir(&r,&d1,"inner",&d2)==0);
 assert(skyrim_child_file(&d2,"file","safe",4,0644)==0);assert(skyrim_child_file(&d2,"file","bad",3,0644)<0&&errno==EEXIST);
 assert(renameat(r.root_fd,"deep",r.root_fd,"deep-moved")==0);assert(mkdirat(r.root_fd,"deep",0700)==0);
 assert(skyrim_child_current(&d2)<0);assert(skyrim_child_file(&d2,"late","x",1,0644)<0);
 skyrim_child_close(&d2);skyrim_child_close(&d1);
 assert(renameat(parent,"PPSA77383",parent,"moved")==0);assert(mkdirat(parent,"PPSA77383",0700)==0);
 assert(skyrim_root_current(&r)<0);assert(skyrim_create_file(&r,"late","x",1,0644)<0);assert(faccessat(r.root_fd,"late",F_OK,0)<0&&errno==ENOENT);
 skyrim_close(&r);close(parent);puts("PASS: exclusive root and files, collision and symlink refusal, displaced inode refusal; no overwrite");return 0;
}
