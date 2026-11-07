/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "admission_observer.h"
#include <stdarg.h>
#include <dirent.h>
#ifndef WINE11_UPSTREAM_ADMISSION_TEST
#error Exact installed-revision upstream fixture source is required
#endif
/* Reuse its isolated /tmp fixture helpers without executing its original main. */
#define main unused_upstream_admission_main
#include WINE11_UPSTREAM_ADMISSION_TEST
#undef main

static int observing, violations, observed_opens, observed_closes, lock_fd_seen = -1;
int __real_open(const char *, int, ...);
int __real_close(int);
ssize_t __real_write(int, const void *, size_t);
int __real_chmod(const char *, mode_t);
int __real_fchmod(int, mode_t);
int __real_mkdir(const char *, mode_t);
int __real_unlink(const char *);
int __real_rename(const char *, const char *);
int __real_chown(const char *, uid_t, gid_t);
int __real_fchown(int, uid_t, gid_t);
int __real_ftruncate(int, off_t);
int __real_truncate(const char *, off_t);
int __real_fsync(int);
int __real_flock(int, int);
int __wrap_open(const char *path_name, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, unsigned); va_end(args); }
    if (observing && ((flags & O_ACCMODE) != O_RDONLY ||
        (flags & (O_CREAT | O_TRUNC | O_APPEND)) || !(flags & O_NOFOLLOW))) {
        ++violations; errno = EPERM; return -1;
    }
    int fd = __real_open(path_name, flags, mode);
    if (observing && fd >= 0) ++observed_opens;
    return fd;
}
int __wrap_close(int fd) { if (observing) ++observed_closes; return __real_close(fd); }
ssize_t __wrap_write(int fd, const void *bytes, size_t size) {
    if (observing) { ++violations; errno = EPERM; return -1; }
    return __real_write(fd, bytes, size);
}
#define WRAP_MUTATOR(name, signature, arguments) \
    int __wrap_##name signature { \
        if (observing) { ++violations; errno = EPERM; return -1; } \
        return __real_##name arguments; \
    }
WRAP_MUTATOR(chmod, (const char *p, mode_t m), (p, m))
WRAP_MUTATOR(fchmod, (int fd, mode_t m), (fd, m))
WRAP_MUTATOR(mkdir, (const char *p, mode_t m), (p, m))
WRAP_MUTATOR(unlink, (const char *p), (p))
WRAP_MUTATOR(rename, (const char *a, const char *b), (a, b))
WRAP_MUTATOR(chown, (const char *p, uid_t u, gid_t g), (p, u, g))
WRAP_MUTATOR(fchown, (int fd, uid_t u, gid_t g), (fd, u, g))
WRAP_MUTATOR(ftruncate, (int fd, off_t n), (fd, n))
WRAP_MUTATOR(truncate, (const char *p, off_t n), (p, n))
WRAP_MUTATOR(fsync, (int fd), (fd))
int __wrap_flock(int fd, int operation) {
    if (observing) {
        if (operation != (LOCK_SH | LOCK_NB) || (fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY) {
            ++violations; errno = EPERM; return -1;
        }
        lock_fd_seen = fd;
    }
    return __real_flock(fd, operation);
}

static uint64_t mix(uint64_t value, const void *data, size_t count) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < count; ++i) value = (value ^ bytes[i]) * UINT64_C(1099511628211);
    return value;
}
static uint64_t snapshot(const char *folder, uint64_t value) {
    DIR *dir = opendir(folder); CHECK(dir);
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char name[2048]; CHECK(snprintf(name, sizeof(name), "%s/%s", folder, entry->d_name) > 0);
        struct stat info; CHECK(lstat(name, &info) == 0);
        value = mix(value, name, strlen(name));
        value = mix(value, &info.st_mode, sizeof(info.st_mode));
        value = mix(value, &info.st_uid, sizeof(info.st_uid));
        value = mix(value, &info.st_gid, sizeof(info.st_gid));
        value = mix(value, &info.st_ino, sizeof(info.st_ino));
        value = mix(value, &info.st_nlink, sizeof(info.st_nlink));
        value = mix(value, &info.st_size, sizeof(info.st_size));
        if (S_ISDIR(info.st_mode)) value = snapshot(name, value);
        else if (S_ISREG(info.st_mode)) {
            int fd = open(name, O_RDONLY); CHECK(fd >= 0);
            char bytes[4096]; ssize_t count;
            while ((count = read(fd, bytes, sizeof(bytes))) > 0) value = mix(value, bytes, (size_t)count);
            CHECK(count == 0 && close(fd) == 0);
        }
    }
    CHECK(closedir(dir) == 0); return value;
}
static Wine11AdmissionReport observe(Fixture *fixture, int granted) {
    uint64_t before = snapshot(fixture->root, UINT64_C(1469598103934665603));
    violations = observed_opens = observed_closes = 0; lock_fd_seen = -1; observing = 1;
    Wine11AdmissionReport result = wine11_admission_observe_test(granted, &fixture->platform);
    observing = 0;
    CHECK(violations == 0 && observed_opens == observed_closes);
    CHECK(before == snapshot(fixture->root, UINT64_C(1469598103934665603)));
    CHECK(!result.safe_to_install && !result.full_kit_artifacts_verified && !result.native_stopped_state_verified);
    CHECK(!strstr(wine11_admission_summary(result.status), "install-220"));
    CHECK(!strstr(wine11_admission_summary(result.status), "/data/"));
    if (lock_fd_seen >= 0) CHECK(fcntl(lock_fd_seen, F_GETFD) < 0 && errno == EBADF);
    return result;
}
static void owners(Fixture *fixture, const char *bottle_digest) {
    char text[2048];
    const char *format = "schema=1\nkind=%s\ninstall_id=install-220\ntitle_id=FAKE78001\nkit_id=kit-hl2\n"
        "files_sha256=%s\nruntime_bundle_id=wine-wow64\nruntime_revision=private-revision\n"
        "runtime_path=runtime/private\nvariant_id=private-variant\ntemplate_path=template/private\n"
        "recipe_id=steam-220-hl2-v1\nrecipe_version=1\n";
    const char *same = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    CHECK(snprintf(text, sizeof(text), format, "title", same) > 0);
    write_file(fixture, "/data/homebrew/FAKE78001/.playden-title-owner", text, strlen(text), 0600);
    CHECK(snprintf(text, sizeof(text), format, "template", bottle_digest ? bottle_digest : same) > 0);
    write_file(fixture, "/data/playden/games/install-220/library/prefixes/game/.playden-template-owner", text, strlen(text), 0600);
}
int main(void) {
    Fixture fixture; int calls = 0; fixture_init(&fixture, &calls); owners(&fixture, NULL);
    Wine11AdmissionReport result = observe(&fixture, 0);
    CHECK(result.status == WINE11_ADMISSION_NO_ACCESS && !observed_opens);
    result = observe(&fixture, 1);
    CHECK(result.status == WINE11_ADMISSION_READ_ONLY_PASSED && result.passed == 15u);
    CHECK(calls == 1);
    char lock_path[1200]; path(lock_path, sizeof(lock_path), &fixture,
        "/data/playden/games/install-220/.playden-control/publication.lock");
    int writer = open(lock_path, O_RDONLY); CHECK(writer >= 0 && flock(writer, LOCK_EX | LOCK_NB) == 0);
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_BUSY);
    CHECK(close(writer) == 0);
    CHECK(unlink(lock_path) == 0);
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_PUBLICATION_REFUSED);
    struct stat info; CHECK(lstat(lock_path, &info) < 0 && errno == ENOENT);
    write_file(&fixture, "/data/playden/games/install-220/.playden-control/publication.lock", "", 0, 0600);
    owners(&fixture, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_OWNER_MISMATCH);
    owners(&fixture, NULL);
    char marker_path[1200]; path(marker_path, sizeof(marker_path), &fixture,
        "/data/homebrew/FAKE78001/.playden-title-owner");
    CHECK(chmod(marker_path, 0644) == 0);
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_OWNER_REFUSED);
    CHECK(chmod(marker_path, 0600) == 0);
    char link_path[1200]; path(link_path, sizeof(link_path), &fixture, "/data/homebrew/FAKE78001/owner-link");
    CHECK(link(marker_path, link_path) == 0);
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_OWNER_REFUSED);
    CHECK(unlink(link_path) == 0);
    fixture.platform.owner_uid = (uint32_t)geteuid() + 1u;
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_DESCRIPTOR_REFUSED);
    fixture.platform.owner_uid = (uint32_t)geteuid();
    strcat(fixture.profile, "# mismatch\n");
    write_file(&fixture, "/data/playden/games/install-220/library/profiles/playden-steam-220.profile",
        fixture.profile, strlen(fixture.profile), 0600);
    CHECK(observe(&fixture, 1).status == WINE11_ADMISSION_PUBLICATION_REFUSED);
    CHECK(!strncmp(fixture.root, "/tmp/pw-publication-native.", strlen("/tmp/pw-publication-native.")));
    fixture_destroy(&fixture);
    puts("PASS: exact native admission and owner coherence; existing read-only shared lock; no writes, repairs, mode/content/inode changes or leaked FDs on success/refusal");
    return 0;
}
