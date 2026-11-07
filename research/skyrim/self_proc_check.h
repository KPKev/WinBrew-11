#ifndef SKYRIM_SELF_PROC_CHECK_H
#define SKYRIM_SELF_PROC_CHECK_H
#include <stddef.h>
/* Never inspect field values until returned length and ABI size agree. */
static int self_proc_check(size_t actual, size_t expected, int structsize,
                           long observed_pid, long expected_pid,
                           unsigned long observed_uid, unsigned long expected_uid) {
    if (actual != expected || structsize < 0 || (size_t)structsize != expected) return 0;
    return observed_pid == expected_pid && observed_uid == expected_uid;
}
#endif
