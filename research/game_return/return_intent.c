/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "return_intent.h"
#include <stddef.h>
#include <string.h>

static int bounded_length(const char *text, size_t limit, size_t *length)
{
    if (!text) return 0;
    for (size_t n = 0; n <= limit; ++n) {
        if (!text[n]) { *length = n; return 1; }
    }
    return 0;
}
static int session_argument(const char *text, size_t length)
{
    if (length != 44 || memcmp(text, "session=", 8)) return 0;
    const char *id = text + 8;
    for (size_t n = 0; n < 36; ++n) {
        const int dash = n == 8 || n == 13 || n == 18 || n == 23;
        const char c = id[n];
        if (dash ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
    }
    return 1;
}
int wine11_return_parse(int argc, const char *const *argv, Wine11ReturnIntent *out)
{
    Wine11ReturnIntent candidate = {0};
    /* Existing Playden title-launch boundary permits no more than eight args. */
    if (!out || argc < 0 || argc > 8 || (argc && !argv)) return -1;
    for (int n = 0; n < argc; ++n) {
        size_t length;
        if (!argv[n] || !argv[n][0]) continue;
        if (!bounded_length(argv[n], 64, &length)) return -1;
        if (length == sizeof(WINE11_RETURN_ARGUMENT) - 1 &&
            !memcmp(argv[n], WINE11_RETURN_ARGUMENT, length)) {
            if (candidate.requested) return -1;
            candidate.requested = 1;
        } else if (session_argument(argv[n], length)) {
            if (candidate.session_argument) return -1;
            candidate.session_argument = argv[n];
        } else return -1;
    }
    *out = candidate;
    return 0;
}
Wine11ReturnExit wine11_return_finish(Wine11ReturnIntent *intent, int was_running,
                                     const Wine11ReturnExitOps *ops)
{
    if (!intent || intent->consumed || !ops || !ops->finish_session ||
        !ops->close_session || !ops->close_publication) return WINE11_RETURN_HOME;
    intent->consumed = 1; /* One attempt per native game process; no restart loop. */
    const int reported = ops->finish_session(ops->context, was_running != 0);
    ops->close_session(ops->context);
    ops->close_publication(ops->context);
    if (!intent->requested || !was_running || reported != 0 || !ops->launch_title)
        return WINE11_RETURN_HOME;
    const uint32_t result = ops->launch_title(ops->context, WINE11_RETURN_TITLE);
    return result & 0x80000000u ? WINE11_RETURN_HOME : WINE11_RETURN_WAIT_FOR_HANDOFF;
}
