/* Rule-mode title admission for the one-request helper: a caller qualifies
 * when its title ID is in a compiled-in range and listed in an allow-list
 * that only root can change. One helper then serves every title a launcher
 * makes on the console, instead of one helper per title. */
#ifndef LAPY_TITLE_RULE_H
#define LAPY_TITLE_RULE_H

#include <sys/types.h>

#define LAPY_TITLE_ID_LENGTH 9u
#define LAPY_TITLE_ALLOW_LIST_MAX 65536u

enum lapy_title_rule_result {
    LAPY_TITLE_RULE_ALLOWED = 0,
    LAPY_TITLE_RULE_OUT_OF_RANGE = 1,   /* not prefix + digits, nine characters */
    LAPY_TITLE_RULE_NOT_LISTED = 2,
    LAPY_TITLE_RULE_LIST_MISSING = 3,   /* the list or its directory can't be read */
    LAPY_TITLE_RULE_LIST_UNSAFE = 4     /* wrong owner, writable by others, a link, too big */
};

/* title is NUL-terminated within LAPY_TITLE_ID_LENGTH + 1 bytes. prefix is
 * four uppercase letters and up to four digits, e.g. "FAKE78". */
int lapy_title_in_range(const char *title, const char *prefix);

/* The allow-list holds one title ID per line, each ending in '\n'. It and its
 * directory must belong to owner (root on the console) and be writable by
 * nobody else; the list must be a regular file with a single link. */
enum lapy_title_rule_result lapy_title_rule_check(const char *title, const char *prefix,
                                                  const char *list_path, uid_t owner);

const char *lapy_title_rule_name(enum lapy_title_rule_result result);

#endif
