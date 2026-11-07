/* Self-only candidate field comparison; no target process or raw output. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define CAPACITY 4096u
static void on_timeout(int sig) { (void)sig; _exit(124); }
static int copy_field(void *out, size_t size, const unsigned char *buffer,
                      size_t length, size_t offset) {
    if (offset > length || size > length-offset) return 0;
    memcpy(out, buffer+offset, size); return 1;
}
static void wipe(void *p, size_t length) {
    volatile unsigned char *bytes = p;
    while (length--) *bytes++ = 0;
}
int main(void) {
    int type = 0; socklen_t type_len = sizeof(type);
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &type_len) ||
        type_len != sizeof(type) || type != SOCK_STREAM) return 1;
    struct sigaction action = {0}; action.sa_handler = on_timeout; sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) return 1;
    alarm(5);
    pid_t own_pid = getpid(); uid_t own_euid = geteuid(), own_ruid = getuid(); gid_t own_rgid = getgid();
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)own_pid};
    unsigned char buffer[CAPACITY] = {0}; size_t length = sizeof(buffer); errno = 0;
    int rc = sysctl(mib, 4, buffer, &length, NULL, 0);
    int bounded = rc == 0 && length > 0 && length <= sizeof(buffer);
    int record_size = 0; pid_t pid = 0; uid_t euid = 0, ruid = 0; gid_t rgid = 0;
    int size_ok = bounded && copy_field(&record_size, sizeof(record_size), buffer, length, offsetof(struct kinfo_proc, ki_structsize)) &&
                  record_size > 0 && (size_t)record_size == length;
    int pid_ok = size_ok && copy_field(&pid, sizeof(pid), buffer, length, offsetof(struct kinfo_proc, ki_pid)) && pid == own_pid;
    int euid_ok = size_ok && copy_field(&euid, sizeof(euid), buffer, length, offsetof(struct kinfo_proc, ki_uid)) && euid == own_euid;
    int ruid_ok = size_ok && copy_field(&ruid, sizeof(ruid), buffer, length, offsetof(struct kinfo_proc, ki_ruid)) && ruid == own_ruid;
    int rgid_ok = size_ok && copy_field(&rgid, sizeof(rgid), buffer, length, offsetof(struct kinfo_proc, ki_rgid)) && rgid == own_rgid;
    wipe(buffer, sizeof(buffer));
    char output[256];
    int n = snprintf(output, sizeof(output), "SELF_CANDIDATE ok=%d bounded=%d length=%zu sdk=%zu size_match=%d pid_match=%d euid_match=%d ruid_match=%d rgid_match=%d\n",
        rc == 0, bounded, bounded ? length : (size_t)0, sizeof(struct kinfo_proc), size_ok, pid_ok, euid_ok, ruid_ok, rgid_ok);
    if (n > 0 && (size_t)n < sizeof(output)) { ssize_t ignored = send(STDOUT_FILENO, output, (size_t)n, MSG_DONTWAIT | MSG_NOSIGNAL); (void)ignored; }
    alarm(0);
    return bounded && size_ok && pid_ok && euid_ok && ruid_ok && rgid_ok ? 0 : 1;
}
