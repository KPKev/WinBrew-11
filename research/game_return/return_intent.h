/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Design prototype only: not part of Wine 11 or the installed game wrapper. */
#ifndef WINE11_RETURN_INTENT_H
#define WINE11_RETURN_INTENT_H
#include <stdint.h>

#define WINE11_RETURN_TITLE "PPSA77381"
#define WINE11_RETURN_ARGUMENT "wine11-return=" WINE11_RETURN_TITLE
#define WINE11_RETURN_FEATURE "wine11-return-on-exit-v1"

typedef struct Wine11ReturnIntent {
    const char *session_argument;
    int requested;
    int consumed;
} Wine11ReturnIntent;

/* Accept legacy session=<canonical UUID> and one fixed opt-in return argument.
 * Empty/NULL argv slots keep the original wrapper's behavior. Output is changed
 * only on success. No arbitrary destination, file, command or URL is accepted. */
int wine11_return_parse(int argc, const char *const *argv, Wine11ReturnIntent *out);

typedef struct Wine11ReturnExitOps {
    void *context;
    /* Return zero after reporting the existing session's exited/failed phase. */
    int (*finish_session)(void *, int was_running);
    void (*close_session)(void *);
    void (*close_publication)(void *);
    /* Must use the existing PS5 title-launch ABI and current foreground user.
     * This callback is passed only the fixed Wine 11 target. */
    uint32_t (*launch_title)(void *, const char *title);
} Wine11ReturnExitOps;

typedef enum Wine11ReturnExit {
    WINE11_RETURN_HOME = 0,
    WINE11_RETURN_WAIT_FOR_HANDOFF = 1
} Wine11ReturnExit;

/* Called only from normal per-game Wine exit, after Wine is done. The actual
 * wrapper remains responsible for diagnostics closure and the final platform
 * action: wait for an accepted handoff, otherwise existing leave_to_home().
 * A crash/PS5 Close Game never calls this normal-exit helper. */
Wine11ReturnExit wine11_return_finish(Wine11ReturnIntent *intent, int was_running,
                                     const Wine11ReturnExitOps *ops);
#endif
