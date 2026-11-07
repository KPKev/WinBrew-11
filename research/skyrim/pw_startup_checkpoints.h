#ifndef PW_STARTUP_CHECKPOINTS_H
#define PW_STARTUP_CHECKPOINTS_H
#include <errno.h>

typedef enum {
    PW_CP_DESCRIPTOR_ENTER, PW_CP_DESCRIPTOR_RETURN,
    PW_CP_HID_ENTER, PW_CP_HID_RETURN,
    PW_CP_LIBRARY_ENTER, PW_CP_LIBRARY_RETURN,
    PW_CP_DATA_ENTER, PW_CP_DATA_RETURN,
    PW_CP_UMASK_ENTER, PW_CP_UMASK_RETURN,
    PW_CP_COUNT
} PwStartupCheckpoint;

typedef int (*PwStartupSink)(const char *, int, int);
/* Static labels and normalized results guarantee bounded, redacted messages.
 * A missing or failing sink must never affect the caller's errno or result. */
static void pw_startup_checkpoint_emit(PwStartupSink sink, PwStartupCheckpoint event,
                                       int result, int helper_error)
{
    static const char *const labels[PW_CP_COUNT] = {
        "descriptor-enter", "descriptor-return", "hid-enter", "hid-return",
        "library-enter", "library-return", "data-enter", "data-return",
        "umask-enter", "umask-return"
    };
    int saved_errno = errno;
    if (sink && event >= 0 && event < PW_CP_COUNT)
        (void)sink(labels[event], result < 0 ? -1 : result > 0 ? 1 : 0,
                   helper_error != 0);
    errno = saved_errno;
}
#endif
