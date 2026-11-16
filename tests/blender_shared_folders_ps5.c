/* Disposable PS5 qualification; never targets a real user profile. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WB_GROUP_ID 365670u
#define WB_DATA_ROOT "/data/winbrew-shared-release-fixture"
static const char *test_title="PPSA77382";
#define TARGET_TITLE test_title
#include "../runtime-patches/blender/winbrew-bridge-helper/source/winbrew_shared_folders.h"
#define CHECK(x) do {if(!(x)){char b[160];int n=snprintf(b,sizeof b,"SHARED_FIXTURE_FAIL line=%d errno=%d expression=%s\n",__LINE__,errno,#x);send(1,b,n,0);return 1;}}while(0)
static int put(const char*p,const char*bytes){int f=open(p,O_WRONLY|O_CREAT|O_EXCL,0600);if(f<0)return -1;size_t n=strlen(bytes);int ok=write(f,bytes,n)==n&&!fsync(f);if(close(f))ok=0;return ok?0:-1;}
static int state(const char*p,unsigned gid,unsigned mode){struct stat s;return !lstat(p,&s)&&s.st_gid==gid&&(s.st_mode&07777)==mode;}
int main(void){CHECK(!seteuid(0));CHECK(!mkdir(WB_DATA_ROOT,0755));CHECK(!wb_reconcile_shared());test_title="FAKE78*";CHECK(wb_reconcile_shared()==ENOENT);test_title="PPSA77382";
const char*parents[]={WB_DATA_ROOT "/WinBrew",WB_DATA_ROOT "/WinBrew/Users",WB_DATA_ROOT "/WinBrew/Users/Default"};for(int i=0;i<3;i++)CHECK(!mkdir(parents[i],0700));
const char*docs=WB_DATA_ROOT "/WinBrew/Users/Default/Documents";const char*desktop=WB_DATA_ROOT "/WinBrew/Users/Default/Desktop";CHECK(!mkdir(docs,0700));CHECK(!mkdir(desktop,0700));
const char*marker=WB_DATA_ROOT "/WinBrew/Users/Default/.winbrew-profile-v1";CHECK(!put(marker,"WINBREW_USER_PROFILE_V1\n" WB_DATA_ROOT "/WinBrew/Users/Default\n"));
const char*valid=WB_DATA_ROOT "/WinBrew/Users/Default/Documents/valid.txt";const char*invalid=WB_DATA_ROOT "/WinBrew/Users/Default/Desktop/invalid.txt";CHECK(!put(valid,"preserved"));CHECK(!put(invalid,"unexpected-owner"));CHECK(!chown(invalid,2,0));CHECK(wb_reconcile_shared()!=0);for(int i=0;i<3;i++)CHECK(state(parents[i],0,0700));CHECK(state(docs,0,0700)&&state(desktop,0,0700)&&state(valid,0,0600));CHECK(!unlink(invalid));
const char*hidden=WB_DATA_ROOT "/WinBrew/Users/Default/Documents/.hidden";CHECK(!put(hidden,"private"));CHECK(!wb_reconcile_shared());CHECK(!wb_reconcile_shared());for(int i=0;i<3;i++)CHECK(state(parents[i],365670,0710));CHECK(state(docs,365670,0770)&&state(desktop,365670,0770)&&state(valid,365670,0660)&&state(hidden,0,0600));
const char*mixed=WB_DATA_ROOT "/WinBrew/Users/Default/Desktop/wine.txt";CHECK(!put(mixed,"wine-owned"));CHECK(!chown(mixed,1,0));CHECK(!wb_reconcile_shared());CHECK(state(mixed,365670,0660));struct stat st;CHECK(!lstat(mixed,&st)&&st.st_uid==1);
const char*trash=WB_DATA_ROOT "/WinBrew/Users/Default/Documents/.winbrew-trash";
const char*box=WB_DATA_ROOT "/WinBrew/Users/Default/Documents/.winbrew-trash/test-box";
const char*origin=WB_DATA_ROOT "/WinBrew/Users/Default/Documents/.winbrew-trash/test-box/.winbrew-original-location";
CHECK(!mkdir(trash,0700)&&!mkdir(box,0700)&&!put(origin,"WINBREW-ORIGIN-1\nfixture"));CHECK(!chown(origin,1,0)&&!chown(box,1,0));CHECK(!wb_reconcile_shared());gid_t recycle_groups[]={0,365670};CHECK(!setgroups(2,recycle_groups));CHECK(state(trash,365670,0770)&&state(box,365670,0770)&&state(origin,365670,0660));CHECK(state(hidden,0,0600));
gid_t groups[]={0,365670};CHECK(!setgroups(2,groups));int f=open(mixed,O_RDONLY);CHECK(f>=0);char bytes[20]={0};CHECK(read(f,bytes,sizeof bytes)==10&&!strcmp(bytes,"wine-owned"));CHECK(!close(f));
CHECK(!unlink(origin)&&!rmdir(box)&&!rmdir(trash));
CHECK(!unlink(mixed)&&!unlink(valid)&&!unlink(hidden)&&!unlink(marker));CHECK(!rmdir(docs)&&!rmdir(desktop));for(int i=2;i>=0;i--)CHECK(!rmdir(parents[i]));CHECK(!rmdir(WB_DATA_ROOT));const char*done="SHARED_FIXTURE_PASS fresh-profile deferred; game missing refused; rollback; idempotence; hidden preserved; mixed owner and recycle metadata shared; cleanup\n";send(1,done,strlen(done),0);return 0;}
