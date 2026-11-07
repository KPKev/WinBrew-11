/* Read-only preview of Vapor's one-time ownership hand-over (Claude, 2026-10-29).
 * Walks exactly the /data roots that src/steam_integration/app_user_identity.cpp
 * adopts and reports, per root, what the update WOULD do. lstat/readdir only:
 * no writes, no ownership or mode changes. (Vapor's private save directory is
 * inside its own sandbox image and is not reachable from here.) */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define APP_UID 1
static void on_alarm(int s) { (void)s; _exit(124); }
static void out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void out(const char *fmt, ...) {
    char b[700]; va_list ap; va_start(ap, fmt); int n = vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof b) n = sizeof b - 1;
    ssize_t ignored = send(STDOUT_FILENO, b, (size_t)n, MSG_NOSIGNAL); (void)ignored;
}
struct tally { unsigned long seen, would_change, already, foreign, symlinks, special, other_dev, errors, unverified, listed; dev_t dev; };

static void walk(const char *path, int depth, struct tally *t) {
    struct stat st;
    if (lstat(path, &st) != 0) { t->errors++; return; }
    t->seen++;
    if (st.st_dev != t->dev) { t->other_dev++; if (t->listed++ < 20) out("X other-device %s\n", path); return; }
    if (S_ISLNK(st.st_mode)) { t->symlinks++; return; }
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) { t->special++; if (t->listed++ < 20) out("X special %s\n", path); return; }
    if (st.st_uid == 0) t->would_change++;
    else if (st.st_uid == APP_UID) t->already++;
    else { t->foreign++; if (t->listed++ < 20) out("X foreign-owner uid=%u %s\n", (unsigned)st.st_uid, path); return; }
    if (!S_ISDIR(st.st_mode) || depth >= 64) return;
    DIR *d = opendir(path);
    /* Same rule as the update (ownership_adopt.cpp, dry run): a private folder
     * that already belongs to the app user cannot be entered by root on this
     * console; an earlier hand-over converted it. Not an error. */
    if (!d && errno == EACCES && st.st_uid == APP_UID) { t->unverified++; return; }
    if (!d) { t->errors++; if (t->listed++ < 20) out("X opendir errno=%d %s\n", errno, path); return; }
    struct dirent *e; char child[1024];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if ((size_t)snprintf(child, sizeof child, "%s/%s", path, e->d_name) >= sizeof child) { t->errors++; continue; }
        walk(child, depth + 1, t);
    }
    closedir(d);
}
static int preview(const char *root) {
    struct tally t = {0}; struct stat st;
    if (lstat(root, &st) != 0) { out("R root=%s missing\n", root); return 0; }
    t.dev = st.st_dev;
    walk(root, 0, &t);
    out("R root=%s mode=%04o owner=%u seen=%lu would_change_root_to_app=%lu already_app=%lu "
        "foreign=%lu symlinks_left_alone=%lu special=%lu other_device=%lu errors=%lu app_private_not_entered=%lu\n",
        root, (unsigned)(st.st_mode & 07777), (unsigned)st.st_uid, t.seen, t.would_change, t.already,
        t.foreign, t.symlinks, t.special, t.other_dev, t.errors, t.unverified);
    return (t.foreign || t.special || t.other_dev || t.errors) ? 1 : 0;
}
int main(void) {
    int type = 0; socklen_t size = sizeof type;
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_STREAM) return 1;
    struct sigaction a = {0}; a.sa_handler = on_alarm; sigemptyset(&a.sa_mask);
    if (sigaction(SIGALRM, &a, NULL)) return 1;
    alarm(50);
    out("OBS adoption-preview read-only uid=%u euid=%u\n", (unsigned)getuid(), (unsigned)geteuid());
    int blocked = 0;
    blocked |= preview("/data/wine11/wine11-games");
    DIR *g = opendir("/data/playden/games");
    if (g) {
        struct dirent *e; char p[1024];
        while ((e = readdir(g)))
            if (!strncmp(e->d_name, "wine11-steam-", 13)) {
                snprintf(p, sizeof p, "/data/playden/games/%s", e->d_name);
                blocked |= preview(p);
            }
        closedir(g);
    }
    char list[4096]; int fd = open("/data/wine11/title-rule/allow-list", O_RDONLY | O_NOFOLLOW);
    ssize_t n = fd >= 0 ? read(fd, list, sizeof list - 1) : -1;
    if (fd >= 0) close(fd);
    if (n < 0) { out("L allow-list unreadable errno=%d\n", errno); blocked = 1; }
    else {
        list[n] = 0;
        char *save = NULL;
        for (char *id = strtok_r(list, "\n", &save); id; id = strtok_r(NULL, "\n", &save)) {
            char p[64];
            out("L allow-list title=%s\n", id);
            if (strlen(id) != 9) { out("L invalid-title-id\n"); blocked = 1; continue; }
            snprintf(p, sizeof p, "/data/homebrew/%s", id);
            blocked |= preview(p);
        }
    }
    struct stat keep;
    const char *untouched[] = {"/data/wine11", "/data/wine11/kits", "/data/wine11/title-rule",
                               "/data/homebrew/FAKE78003", "/data/homebrew/PPSA77381", "/data/homebrew/PPSA77382"};
    for (unsigned i = 0; i < sizeof untouched / sizeof untouched[0]; i++)
        if (lstat(untouched[i], &keep) == 0)
            out("U untouched=%s owner=%u mode=%04o\n", untouched[i], (unsigned)keep.st_uid, (unsigned)(keep.st_mode & 07777));
    alarm(0);
    out("OBS done update_would=%s\n", blocked ? "STOP-and-stay-root" : "proceed");
    return 0;
}
