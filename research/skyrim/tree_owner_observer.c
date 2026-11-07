/* Read-only ownership census for Wine 11 / Playden title trees (Claude, 2026-10-29).
 * lstat only: no file contents read, no writes, no credential changes.
 * Output goes to the elfldr stdout socket. Bounded by depth, entry count and a 20 s alarm. */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_DEPTH 6
#define MAX_LISTED 60      /* per root: individual lines for entries not owned by root's own uid */
#define MAX_VISITED 200000 /* per root */

static void timeout_handler(int s) { (void)s; _exit(124); }
static void out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void out(const char *fmt, ...) {
    char buf[600]; va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof buf) n = sizeof buf - 1;
    ssize_t ignored = send(STDOUT_FILENO, buf, (size_t)n, MSG_NOSIGNAL); (void)ignored;
}
static const char *kind(mode_t m) { return S_ISDIR(m) ? "d" : S_ISREG(m) ? "f" : S_ISLNK(m) ? "l" : "o"; }

struct census { unsigned long visited, uid0, uid1, other, errors, listed; int truncated; };

static void walk(const char *root, const char *path, int depth, struct census *c, int list_all) {
    struct stat st;
    if (c->visited >= MAX_VISITED) { c->truncated = 1; return; }
    if (lstat(path, &st) != 0) { c->errors++; return; }
    c->visited++;
    if (st.st_uid == 0) c->uid0++; else if (st.st_uid == 1) c->uid1++; else c->other++;
    if ((list_all || depth <= 2) && c->listed < MAX_LISTED) {
        c->listed++;
        out("E root=%s rel=%s t=%s uid=%u gid=%u mode=%04o links=%u\n", root,
            path[strlen(root)] ? path + strlen(root) : "/", kind(st.st_mode),
            (unsigned)st.st_uid, (unsigned)st.st_gid, (unsigned)(st.st_mode & 07777), (unsigned)st.st_nlink);
    }
    if (!S_ISDIR(st.st_mode) || depth >= MAX_DEPTH) return;
    DIR *d = opendir(path);
    if (!d) { c->errors++; return; }
    struct dirent *e;
    char child[1024];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if ((size_t)snprintf(child, sizeof child, "%s/%s", path, e->d_name) >= sizeof child) { c->errors++; continue; }
        walk(root, child, depth + 1, c, list_all);
    }
    closedir(d);
}

static void census(const char *root, int list_all) {
    struct census c = {0};
    walk(root, root, 0, &c, list_all);
    out("S root=%s visited=%lu uid0=%lu uid1=%lu other=%lu errors=%lu truncated=%d\n",
        root, c.visited, c.uid0, c.uid1, c.other, c.errors, c.truncated);
}

static void list_dir(const char *path) {  /* depth-1 listing to locate data roots */
    DIR *d = opendir(path); struct dirent *e; struct stat st; char child[1024];
    if (!d) { out("L path=%s open=0\n", path); return; }
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(child, sizeof child, "%s/%s", path, e->d_name);
        if (lstat(child, &st) == 0)
            out("L path=%s t=%s uid=%u gid=%u mode=%04o\n", child, kind(st.st_mode),
                (unsigned)st.st_uid, (unsigned)st.st_gid, (unsigned)(st.st_mode & 07777));
    }
    closedir(d);
}

int main(void) {
    int type = 0; socklen_t size = sizeof type;
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_STREAM) return 1;
    struct sigaction a = {0}; a.sa_handler = timeout_handler; sigemptyset(&a.sa_mask);
    if (sigaction(SIGALRM, &a, NULL)) return 1;
    alarm(20);
    out("OBS tree-owner ready uid=%u euid=%u\n", (unsigned)getuid(), (unsigned)geteuid());
    /* Title folders: list everything (they are small). */
    census("/data/homebrew/FAKE78003", 1);
    census("/data/homebrew/FAKE78004", 1);
    census("/data/homebrew/FAKE78005", 1);
    /* Locate data roots. */
    list_dir("/data/playden");
    list_dir("/data/playden/games");
    list_dir("/data/playden-games");
    list_dir("/data/wine11");
    list_dir("/data/homebrew/PPSA77381");
    list_dir("/data/homebrew/PPSA77382");
    /* Data roots: summaries plus top two levels. */
    census("/data/playden-games/1089130", 0);
    census("/data/playden/games", 0);
    census("/data/wine11", 0);
    alarm(0);
    out("OBS done\n");
    return 0;
}
