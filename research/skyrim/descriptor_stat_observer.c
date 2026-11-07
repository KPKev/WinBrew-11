/* Fixed-path metadata-only observer. No file contents, writes or elevation. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#ifndef OBS_TITLE_ROOT
#define OBS_TITLE_ROOT "/data/homebrew/FAKE78005"
#endif
#ifndef OBS_DESCRIPTOR
#define OBS_DESCRIPTOR "/data/homebrew/FAKE78005/game-launch.json"
#endif
static void timeout_handler(int signal_number) { (void)signal_number; _exit(124); }
static int same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_mode == b->st_mode && a->st_uid == b->st_uid &&
           a->st_gid == b->st_gid && a->st_nlink == b->st_nlink;
}
static const char *kind(mode_t mode) {
    return S_ISDIR(mode) ? "directory" : S_ISREG(mode) ? "regular" :
           S_ISLNK(mode) ? "symlink" : "other";
}
static void emit(const char *name, int first_ok, const struct stat *a,
                 int second_ok, const struct stat *b) {
    char output[320];
    int n;
    if (!first_ok || !second_ok)
        n = snprintf(output, sizeof(output), "STAT item=%s first=%d second=%d stable=0\n", name, first_ok, second_ok);
    else
        n = snprintf(output, sizeof(output), "STAT item=%s first=1 second=1 type=%s uid=%llu gid=%llu mode=%04o links=%llu dev=%llu ino=%llu stable=%d\n",
            name, kind(a->st_mode), (unsigned long long)a->st_uid, (unsigned long long)a->st_gid,
            (unsigned)(a->st_mode & 07777), (unsigned long long)a->st_nlink,
            (unsigned long long)a->st_dev, (unsigned long long)a->st_ino, same(a,b));
    if (n > 0 && (size_t)n < sizeof(output)) {
        ssize_t ignored = send(STDOUT_FILENO, output, (size_t)n, MSG_DONTWAIT | MSG_NOSIGNAL);
        (void)ignored;
    }
}
int main(void) {
    int type = 0; socklen_t size = sizeof(type);
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &size) ||
        size != sizeof(type) || type != SOCK_STREAM) return 1;
    struct sigaction action = {0}; action.sa_handler = timeout_handler; sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) return 1;
    alarm(5);
    struct stat p1, p2, f1, f2;
    int a = lstat(OBS_TITLE_ROOT, &p1) == 0;
    int b = a && S_ISDIR(p1.st_mode) && lstat(OBS_DESCRIPTOR, &f1) == 0;
    int c = a && S_ISDIR(p1.st_mode) && lstat(OBS_DESCRIPTOR, &f2) == 0;
    int d = lstat(OBS_TITLE_ROOT, &p2) == 0;
    emit("parent", a, &p1, d, &p2);
    emit("descriptor", b, &f1, c, &f2);
    alarm(0);
    return 0;
}
