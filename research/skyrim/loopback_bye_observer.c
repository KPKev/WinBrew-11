/* Standalone diagnostic prototype: bounded loopback-only, no file access. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#ifndef OBS_PORT
#define OBS_PORT 9300
#endif
#ifndef OBS_SECONDS
#define OBS_SECONDS 30
#endif
#ifndef OBS_MAX_BYTES
#define OBS_MAX_BYTES 16384
#endif
#define LINE_MAXIMUM 1024
static long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return -1;
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static int ready(int fd, long long deadline) {
    for (;;) {
        long long now = now_ms();
        if (now < 0 || fd < 0 || fd >= FD_SETSIZE) return -1;
        long long remaining = deadline - now;
        if (remaining <= 0) return 0;
        struct timeval tv = {(long)(remaining / 1000), (long)((remaining % 1000) * 1000)};
        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
#ifdef OBS_TEST_SELECT_ERROR
        (void)tv; (void)fds; errno = EIO; int rc = -1;
#else
        int rc = select(fd + 1, &fds, NULL, NULL, &tv);
#endif
        if (rc >= 0) return rc;
        if (errno != EINTR) return -1;
    }
}
static int permitted(const char *name) {
    static const char *const list[] = {"descriptor-enter", "descriptor-return", "hid-enter", "hid-return", "library-enter", "library-return", "data-enter", "data-return", "umask-enter", "umask-return"};
    for (size_t i = 0; i < sizeof(list)/sizeof(list[0]); ++i)
        if (!strcmp(name, list[i])) return 1;
    return 0;
}
/* A bounded nonblocking write may be dropped or partial; never retry. */
static void emit(const char *text) { ssize_t ignored = send(STDOUT_FILENO, text, strlen(text), MSG_DONTWAIT | MSG_NOSIGNAL); (void)ignored; }
static int number_then_tab(const char **cursor) {
    const char *p = *cursor; uint64_t n = 0; int digits = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p++ - '0');
        if (n > (UINT64_MAX - digit) / 10) return 0;
        n = n * 10 + digit; ++digits;
    }
    if (!digits || *p != '\t') return 0;
    *cursor = p + 1; return 1;
}
static int bye_reason(const char *s) {
    static const char prefix[] = "BYE seq=";
    if (strncmp(s, prefix, sizeof(prefix)-1)) return 0;
    const char *p = s + sizeof(prefix)-1;
    uint64_t seq = 0; int digits = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p++ - '0');
        if (seq > (UINT64_MAX - digit) / 10) return 0;
        seq = seq * 10 + digit; ++digits;
    }
    if (!digits || strncmp(p, " reason=", 8)) return 0;
    p += 8;
    if (!strcmp(p, "invalid-descriptor")) { emit("OBS bye=invalid-descriptor\n"); return 1; }
    if (!strcmp(p, "invalid-per-game-arguments")) { emit("OBS bye=invalid-per-game-arguments\n"); return 1; }
    if (!strcmp(p, "per-game-bootstrap")) { emit("OBS bye=per-game-bootstrap\n"); return 1; }
    return 0;
}
static void line(const char *s, int *hello_seen) {
    static const char hello[] = "HELLO ps5log/1 title=FAKE78005 app=prospero-win-wine64 ";
    if (!*hello_seen) {
        if (!strncmp(s, hello, sizeof(hello)-1)) { *hello_seen = 1; emit("OBS hello=matched-unverified\n"); }
        return;
    }
    if (bye_reason(s)) return;
    const char *p = s;
    if (!number_then_tab(&p) || !number_then_tab(&p)) return;
    static const char prefix[] = "INFO\tPW_STARTUP checkpoint=";
    if (strncmp(p, prefix, sizeof(prefix)-1)) return;
    p += sizeof(prefix)-1;
    char name[24]; size_t n = 0;
    while ((*p >= 'a' && *p <= 'z') || *p == '-') {
        if (n == sizeof(name)-1) return;
        name[n++] = *p++;
    }
    name[n] = 0;
    if (!permitted(name)) return;
    static const char result_prefix[] = " result=";
    if (strncmp(p, result_prefix, sizeof(result_prefix)-1)) return;
    p += sizeof(result_prefix)-1;
    int result;
    if (!strncmp(p, "-1", 2)) { result = -1; p += 2; }
    else if (*p == '0' || *p == '1') { result = *p++ - '0'; }
    else return;
    static const char helper_prefix[] = " helper_error=";
    if (strncmp(p, helper_prefix, sizeof(helper_prefix)-1)) return;
    p += sizeof(helper_prefix)-1;
    if ((p[0] != '0' && p[0] != '1') || p[1] != 0) return;
    char output[128];
    int length = snprintf(output, sizeof(output), "OBS checkpoint=%s result=%d helper_error=%c\n", name, result, *p);
    if (length > 0 && (size_t)length < sizeof(output)) emit(output);
}
int main(void) {
    int server = -1, client = -1, code = 1, identity = 0, connections = 0; const char *reason = "setup-error"; size_t used = 0, total = 0; int overlong = 0; char buf[LINE_MAXIMUM + 1];
    int socket_type = 0; socklen_t socket_type_len = sizeof(socket_type);
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &socket_type, &socket_type_len) ||
        socket_type_len != sizeof(socket_type) || socket_type != SOCK_STREAM) return 1;
    long long start = now_ms(); if (start < 0) { emit("OBS error=clock\n"); return 1; }
    long long deadline = start + OBS_SECONDS * 1000LL;
    server = socket(AF_INET, SOCK_STREAM, 0); if (server < 0) goto done;
    int reuse = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) goto done;
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr)); addr.sin_family = AF_INET;
#ifdef OBS_SOCKADDR_HAS_LEN
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_addr.s_addr = htonl(0x7f000001u); addr.sin_port = htons(OBS_PORT);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) || listen(server, 1)) goto done;
    emit("OBS ready=1\n");
    int wait_result = ready(server, deadline);
    if (wait_result <= 0) {
        reason = wait_result == 0 ? "timeout" : "wait-error";
        code = wait_result == 0 ? 0 : 1; goto done;
    }
    client = accept(server, NULL, NULL);
    if (client < 0) { reason = "accept-error"; goto done; }
    connections = 1;
    while (total < OBS_MAX_BYTES) {
        int r = ready(client, deadline); if (r <= 0) { reason = r == 0 ? "timeout" : "wait-error"; code = r == 0 ? 0 : 1; break; }
        char chunk[256]; size_t want = sizeof(chunk); if (want > OBS_MAX_BYTES - total) want = OBS_MAX_BYTES - total;
        ssize_t n = recv(client, chunk, want, 0); if (n <= 0) { reason = n == 0 ? "eof" : "receive-error"; code = n == 0 ? 0 : 1; break; }
        total += (size_t)n;
        for (ssize_t i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)chunk[i];
            if (c == '\n') { if (!overlong) { buf[used] = 0; line(buf, &identity); } used = 0; overlong = 0; }
            else if (c == 0) overlong = 1;
            else if (used < LINE_MAXIMUM) buf[used++] = (char)c;
            else overlong = 1;
        }
        if (total == OBS_MAX_BYTES) { reason = "byte-limit"; code = 0; }
    }
done:
    if (client >= 0) close(client);
    if (server >= 0) close(server);
    char summary[80];
    int length = snprintf(summary, sizeof(summary), "OBS reason=%s connections=%d bytes=%zu\n", reason, connections, total);
    if (length > 0 && (size_t)length < sizeof(summary)) emit(summary);
    return code;
}
