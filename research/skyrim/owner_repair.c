/* Ownership repair for a Vapor-published title (Claude, 2026-10-29).
 * Makes a root-published title look exactly like a Playden-published one:
 * every entry under the fixed roots goes from uid 0 to the app user (uid 1),
 * gid and modes untouched. Optionally makes the shared Vapor games folder
 * traversable (0700 -> 0711). Never follows symlinks, never leaves the root's
 * device, refuses unexpected owners. REPAIR_APPLY=0 is a dry run.
 * Rollback: lchown back to 0:0 under the same roots and chmod 0700 (the census
 * evidence shows every entry was 0:0 before). */
#define _DEFAULT_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef REPAIR_APPLY
#define REPAIR_APPLY 0
#endif
/* v2: post-order walk (see visit). */
#ifndef ROOT_TITLE
#define ROOT_TITLE "/data/homebrew/FAKE78005"
#endif
#ifndef ROOT_DATA
#define ROOT_DATA "/data/playden/games/wine11-steam-489830"
#endif
#ifndef TRAVERSE_DIR
#define TRAVERSE_DIR "/data/wine11/wine11-games"
#endif
#define APP_UID 1
#define MAX_ENTRIES 50000
#define MAX_DEPTH 24
#define MAX_EXCEPTIONS 40

static void timeout_handler(int s) { (void)s; _exit(124); }
static void out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void out(const char *fmt, ...) {
    char buf[700]; va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof buf) n = sizeof buf - 1;
    ssize_t ignored = send(STDOUT_FILENO, buf, (size_t)n, MSG_NOSIGNAL); (void)ignored;
}

struct tally { unsigned long seen, already, changed, verified, symlinks, refused, errors, exceptions; dev_t dev; };

static void note(struct tally *t, const char *what, const char *path, int err) {
    if (t->exceptions++ < MAX_EXCEPTIONS) out("X %s path=%s errno=%d\n", what, path, err);
}

static int flip(const char *path, const struct stat *st, struct tally *t) {
    if (!REPAIR_APPLY) { t->changed++; return 0; }
    if (lchown(path, APP_UID, (gid_t)-1) != 0) { t->errors++; note(t, "lchown", path, errno); return -1; }
    struct stat after;
    t->changed++;
    if (lstat(path, &after) == 0 && after.st_uid == APP_UID && after.st_gid == st->st_gid &&
        (after.st_mode & 07777) == (st->st_mode & 07777) && after.st_ino == st->st_ino) t->verified++;
    else { t->errors++; note(t, "verify", path, errno); return -1; }
    return 0;
}

/* Post-order: a directory's contents are repaired while it is still root's,
 * and the directory itself last. On this console root cannot enter a 0700
 * directory owned by another user, so the order matters. A directory that an
 * earlier run already flipped is handed back to root (its prior owner) for the
 * walk and flipped again afterwards. */
static void visit(const char *path, int depth, struct tally *t) {
    struct stat st;
    if (t->seen >= MAX_ENTRIES) { t->errors++; note(t, "entry-limit", path, 0); return; }
    if (lstat(path, &st) != 0) { t->errors++; note(t, "lstat", path, errno); return; }
    t->seen++;
    if (st.st_dev != t->dev) { t->refused++; note(t, "other-device", path, 0); return; }
    if (S_ISLNK(st.st_mode)) { t->symlinks++; note(t, "symlink-left-alone", path, 0); return; }
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) { t->refused++; note(t, "not-file-or-dir", path, 0); return; }
    if (st.st_uid != 0 && st.st_uid != APP_UID) { t->refused++; note(t, "unexpected-owner", path, (int)st.st_uid); return; }
    if (!S_ISDIR(st.st_mode)) {
        if (st.st_uid == APP_UID) t->already++; else flip(path, &st, t);
        return;
    }
    if (depth >= MAX_DEPTH) { t->errors++; note(t, "depth-limit", path, 0); return; }
    int handed_back = 0;
    DIR *d = opendir(path);
    if (!d && errno == EACCES && st.st_uid == APP_UID && REPAIR_APPLY) {
        if (lchown(path, 0, (gid_t)-1) == 0) { handed_back = 1; d = opendir(path); }
    }
    if (!d) { t->errors++; note(t, "opendir", path, errno); return; }
    struct dirent *e; char child[1024];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if ((size_t)snprintf(child, sizeof child, "%s/%s", path, e->d_name) >= sizeof child) {
            t->errors++; note(t, "path-too-long", path, 0); continue; }
        visit(child, depth + 1, t);
    }
    closedir(d);
    if (st.st_uid == APP_UID && !handed_back) { t->already++; return; }
    flip(path, &st, t);
}

static int repair_root(const char *root) {
    struct tally t = {0};
    struct stat st;
    if (lstat(root, &st) != 0 || !S_ISDIR(st.st_mode)) { out("R root=%s missing-or-not-dir errno=%d\n", root, errno); return 1; }
    t.dev = st.st_dev;
    visit(root, 0, &t);
    out("R root=%s apply=%d seen=%lu already_app=%lu %s=%lu verified=%lu symlinks=%lu refused=%lu errors=%lu\n",
        root, REPAIR_APPLY, t.seen, t.already, REPAIR_APPLY ? "changed" : "would_change", t.changed,
        t.verified, t.symlinks, t.refused, t.errors);
    return (t.refused || t.errors) ? 1 : 0;
}

static int traverse_fix(const char *dir) {
    struct stat st;
    if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) { out("T dir=%s missing errno=%d\n", dir, errno); return 1; }
    unsigned mode = st.st_mode & 07777;
    if (st.st_uid != 0) { out("T dir=%s unexpected-owner=%u\n", dir, (unsigned)st.st_uid); return 1; }
    if ((mode & 0011) == 0011) { out("T dir=%s mode=%04o already-traversable\n", dir, mode); return 0; }
    if (mode != 0700) { out("T dir=%s mode=%04o unexpected-mode\n", dir, mode); return 1; }
    if (!REPAIR_APPLY) { out("T dir=%s mode=0700 would_set=0711\n", dir); return 0; }
    if (chmod(dir, 0711) != 0) { out("T dir=%s chmod errno=%d\n", dir, errno); return 1; }
    if (lstat(dir, &st) != 0 || (st.st_mode & 07777) != 0711) { out("T dir=%s verify-failed\n", dir); return 1; }
    out("T dir=%s mode=0711 set=1\n", dir);
    return 0;
}

int main(void) {
    int type = 0; socklen_t size = sizeof type;
    if (getsockopt(STDOUT_FILENO, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_STREAM) return 1;
    struct sigaction a = {0}; a.sa_handler = timeout_handler; sigemptyset(&a.sa_mask);
    if (sigaction(SIGALRM, &a, NULL)) return 1;
    alarm(40);
    out("OBS owner-repair ready apply=%d uid=%u euid=%u\n", REPAIR_APPLY, (unsigned)getuid(), (unsigned)geteuid());
    if (geteuid() != 0) { out("OBS refused: needs euid 0\n"); return 1; }
    int bad = 0;
    bad |= repair_root(ROOT_TITLE);
    bad |= repair_root(ROOT_DATA);
    bad |= traverse_fix(TRAVERSE_DIR);
    alarm(0);
    out("OBS done result=%s\n", bad ? "incomplete" : "ok");
    return bad;
}
