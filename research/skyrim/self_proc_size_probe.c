/* Self-only size query: no process record is requested or emitted. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#define SIZE_CAP 65536u
static void on_timeout(int sig) { (void)sig; _exit(124); }
int main(void) {
    int type = 0; socklen_t type_len = sizeof(type);
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &type_len) ||
        type_len != sizeof(type) || type != SOCK_STREAM) return 1;
    struct sigaction action = {0}; action.sa_handler = on_timeout; sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) return 1;
    alarm(5);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
    size_t required = 0;
    errno = 0;
    int rc = sysctl(mib, 4, NULL, &required, NULL, 0);
    int error = rc == 0 ? 0 : errno;
    int in_range = rc == 0 && required > 0 && required <= SIZE_CAP;
    char output[192];
    int n = snprintf(output, sizeof(output), "SELF_SIZE ok=%d errno=%d in_range=%d size=%zu expected=%zu\n",
                     rc == 0, error, in_range, in_range ? required : (size_t)0, sizeof(struct kinfo_proc));
    if (n > 0 && (size_t)n < sizeof(output)) {
        ssize_t ignored = send(STDOUT_FILENO, output, (size_t)n, MSG_DONTWAIT | MSG_NOSIGNAL);
        (void)ignored;
    }
    alarm(0);
    return in_range ? 0 : 1;
}
