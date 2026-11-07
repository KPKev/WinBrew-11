#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "self_proc_check.h"
static void on_timeout(int sig) { (void)sig; _exit(124); }
int main(void) {
    int type = 0; socklen_t type_len = sizeof(type);
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &type_len) ||
        type_len != sizeof(type) || type != SOCK_STREAM) return 1;
    struct sigaction action = {0}; action.sa_handler = on_timeout; sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) return 1;
    alarm(5);
    pid_t pid = getpid(); uid_t uid = geteuid();
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)pid};
    struct kinfo_proc info; memset(&info, 0, sizeof(info)); size_t length = sizeof(info);
    errno = 0;
    int rc = sysctl(mib, 4, &info, &length, NULL, 0);
    int error = rc == 0 ? 0 : errno;
    /* Require the full ABI-sized response before interpreting any field. */
    int abi = rc == 0 && length == sizeof(info) && info.ki_structsize >= 0 &&
              (size_t)info.ki_structsize == sizeof(info);
    int pid_match = abi && info.ki_pid == pid;
    int uid_match = abi && info.ki_uid == uid;
    int valid = abi && self_proc_check(length, sizeof(info), info.ki_structsize,
        info.ki_pid, pid, info.ki_uid, uid);
    char output[192];
    int n = snprintf(output, sizeof(output), "SELF_PROC sysctl_ok=%d errno=%d abi=%d pid_match=%d euid_match=%d valid=%d\n",
                     rc == 0, error, abi, pid_match, uid_match, valid);
    if (n > 0 && (size_t)n < sizeof(output)) { ssize_t ignored = send(STDOUT_FILENO, output, (size_t)n, MSG_DONTWAIT | MSG_NOSIGNAL); (void)ignored; }
    alarm(0);
    return valid ? 0 : 1;
}
