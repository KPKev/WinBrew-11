#include "title_rule.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int lapy_title_in_range(const char *title, const char *prefix)
{
    size_t length = strlen(prefix);
    if (length < 4 || length >= LAPY_TITLE_ID_LENGTH) return 0;
    for (size_t i = 0; i < 4; ++i)
        if (prefix[i] < 'A' || prefix[i] > 'Z') return 0;
    for (size_t i = 4; i < length; ++i)
        if (prefix[i] < '0' || prefix[i] > '9') return 0;
    if (strncmp(title, prefix, length)) return 0;
    for (size_t i = length; i < LAPY_TITLE_ID_LENGTH; ++i)
        if (title[i] < '0' || title[i] > '9') return 0;
    return title[LAPY_TITLE_ID_LENGTH] == 0;
}

static int private_to(const struct stat *st, uid_t owner)
{
    return st->st_uid == owner && !(st->st_mode & (S_IWGRP | S_IWOTH));
}

static int directory_safe(const char *list_path, uid_t owner)
{
    char directory[512];
    const char *slash = strrchr(list_path, '/');
    struct stat st;
    size_t length;

    if (list_path[0] != '/' || !slash) return LAPY_TITLE_RULE_LIST_UNSAFE;
    length = slash == list_path ? 1 : (size_t)(slash - list_path);
    if (length >= sizeof(directory)) return LAPY_TITLE_RULE_LIST_UNSAFE;
    memcpy(directory, list_path, length);
    directory[length] = 0;
    if (lstat(directory, &st)) return LAPY_TITLE_RULE_LIST_MISSING;
    if (!S_ISDIR(st.st_mode) || !private_to(&st, owner)) return LAPY_TITLE_RULE_LIST_UNSAFE;
    return LAPY_TITLE_RULE_ALLOWED;
}

static int listed(const char *list, size_t size, const char *title)
{
    for (size_t at = 0; at < size;) {
        const char *end = memchr(list + at, '\n', size - at);
        if (!end) return 0;   /* an unterminated last line is a partial write */
        size_t length = (size_t)(end - (list + at));
        if (length == LAPY_TITLE_ID_LENGTH && !memcmp(list + at, title, length)) return 1;
        at += length + 1;
    }
    return 0;
}

enum lapy_title_rule_result lapy_title_rule_check(const char *title, const char *prefix,
                                                  const char *list_path, uid_t owner)
{
    static char list[LAPY_TITLE_ALLOW_LIST_MAX];
    struct stat st;
    size_t used = 0;
    int result, fd;

    if (!lapy_title_in_range(title, prefix)) return LAPY_TITLE_RULE_OUT_OF_RANGE;
    if ((result = directory_safe(list_path, owner))) return result;
    fd = open(list_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        return errno == ELOOP || errno == EMLINK ? LAPY_TITLE_RULE_LIST_UNSAFE :
                                                   LAPY_TITLE_RULE_LIST_MISSING;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_nlink != 1 || !private_to(&st, owner) ||
        st.st_size < 0 || (unsigned long long)st.st_size > LAPY_TITLE_ALLOW_LIST_MAX) {
        close(fd);
        return LAPY_TITLE_RULE_LIST_UNSAFE;
    }
    for (;;) {
        ssize_t got = read(fd, list + used, sizeof(list) - used);
        if (got < 0) { close(fd); return LAPY_TITLE_RULE_LIST_MISSING; }
        if (!got) break;
        used += (size_t)got;
        if (used == sizeof(list)) { close(fd); return LAPY_TITLE_RULE_LIST_UNSAFE; }
    }
    close(fd);
    return listed(list, used, title) ? LAPY_TITLE_RULE_ALLOWED : LAPY_TITLE_RULE_NOT_LISTED;
}

const char *lapy_title_rule_name(enum lapy_title_rule_result result)
{
    switch (result) {
    case LAPY_TITLE_RULE_ALLOWED: return "allowed";
    case LAPY_TITLE_RULE_OUT_OF_RANGE: return "out-of-range";
    case LAPY_TITLE_RULE_NOT_LISTED: return "not-listed";
    case LAPY_TITLE_RULE_LIST_MISSING: return "list-missing";
    case LAPY_TITLE_RULE_LIST_UNSAFE: return "list-unsafe";
    }
    return "unknown";
}
