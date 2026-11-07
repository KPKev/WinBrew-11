/* Read-only comparison: exactly three fixed title roots and descriptors. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#ifdef OBS_TEST_FIXTURES
#ifndef OBS_TEST_ROOT
#error OBS_TEST_ROOT is required for host fixtures
#endif
#define BASE OBS_TEST_ROOT
#else
#define BASE "/data/homebrew"
#endif
static const struct { const char *title, *root, *descriptor; } targets[] = {
    {"FAKE78003", BASE "/FAKE78003", BASE "/FAKE78003/game-launch.json"},
    {"FAKE78004", BASE "/FAKE78004", BASE "/FAKE78004/game-launch.json"},
    {"FAKE78005", BASE "/FAKE78005", BASE "/FAKE78005/game-launch.json"}
};
static void timeout_handler(int sig) { (void)sig; _exit(124); }
static int same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_mode == b->st_mode &&
           a->st_uid == b->st_uid && a->st_gid == b->st_gid && a->st_nlink == b->st_nlink;
}
static const char *kind(mode_t mode) {
    return S_ISDIR(mode) ? "directory" : S_ISREG(mode) ? "regular" : S_ISLNK(mode) ? "symlink" : "other";
}
static void emit(const char *title, const char *item, int first, const struct stat *a,
                 int second, const struct stat *b) {
    char output[360]; int n;
    if (!first || !second)
        n = snprintf(output,sizeof(output),"COMPARE title=%s item=%s first=%d second=%d stable=0\n",title,item,first,second);
    else
        n = snprintf(output,sizeof(output),"COMPARE title=%s item=%s first=1 second=1 type=%s uid=%llu gid=%llu mode=%04o links=%llu dev=%llu ino=%llu stable=%d\n",
            title,item,kind(a->st_mode),(unsigned long long)a->st_uid,(unsigned long long)a->st_gid,
            (unsigned)(a->st_mode & 07777),(unsigned long long)a->st_nlink,
            (unsigned long long)a->st_dev,(unsigned long long)a->st_ino,same(a,b));
    if (n>0 && (size_t)n<sizeof(output)) { ssize_t ignored=send(STDOUT_FILENO,output,(size_t)n,MSG_DONTWAIT|MSG_NOSIGNAL); (void)ignored; }
}
int main(void) {
    int type=0; socklen_t len=sizeof(type);
    if (getsockopt(STDOUT_FILENO,SOL_SOCKET,SO_TYPE,&type,&len) || len!=sizeof(type) || type!=SOCK_STREAM) return 1;
    struct sigaction action={0}; action.sa_handler=timeout_handler; sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM,&action,NULL)) return 1;
    alarm(5);
    for (size_t i=0;i<sizeof(targets)/sizeof(targets[0]);i++) {
        struct stat p1,p2,f1,f2;
        int a=lstat(targets[i].root,&p1)==0;
        int b=a && S_ISDIR(p1.st_mode) && lstat(targets[i].descriptor,&f1)==0;
        int c=a && S_ISDIR(p1.st_mode) && lstat(targets[i].descriptor,&f2)==0;
        int d=lstat(targets[i].root,&p2)==0;
        emit(targets[i].title,"parent",a,&p1,d,&p2);
        emit(targets[i].title,"descriptor",b,&f1,c,&f2);
    }
    alarm(0); return 0;
}
