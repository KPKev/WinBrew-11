#include "../research/skyrim/pw_startup_checkpoints.h"
#include "../build/return-wrapper/source/native/pw_data_mount.h"
#include <assert.h>
#include <string.h>
#include <sys/types.h>
static int calls, fail_sink, saw_fallback, helpers, is_visible = 1;
static uid_t uid = 1;
uid_t geteuid(void) { return uid; }
uid_t getuid(void) { return uid; }
int seteuid(uid_t v) { uid = v; return 0; }
int setreuid(uid_t a, uid_t b) { (void)b; uid = a; return 0; }
static int sink(const char *label, int result, int helper_error) {
    calls++; assert(strlen(label) <= 17); assert(result >= -1 && result <= 1);
    assert(helper_error == 0 || helper_error == 1);
    if (!strcmp(label, "data-return") && result == 0 && helper_error == 1) saw_fallback++;
    errno = EIO; return fail_sink ? -1 : 0;
}
static int request(int32_t pid) { (void)pid; helpers++; errno = ETIMEDOUT; return -1; }
static int visible(void) { return is_visible; }
static int elevated(void) { return 0; }
static void sleep_ms(int ms) { (void)ms; assert(0); }
int main(void) {
    const PwDataMountOps ops = {request, visible, sleep_ms, elevated};
    PwDataMountResult detail;
    for (int failure = 0; failure != 2; failure++) {
        fail_sink = failure; int before = helpers;
        errno = ERANGE;
        pw_startup_checkpoint_emit(sink, PW_CP_DATA_ENTER, 0, 0);
        assert(errno == ERANGE);
        int status = pw_data_mount_request_with(&ops, 7, 0, &detail);
        int saved = errno;
        pw_startup_checkpoint_emit(sink, PW_CP_DATA_RETURN, status, detail.helper_errno);
        assert(errno == saved && status == 0 && detail.helper_errno == ETIMEDOUT);
        assert(detail.data_before && detail.data_after && !detail.helper_completed);
        assert(helpers == before + 1 && uid == 1);
    }
    assert(saw_fallback == 2 && calls == 4);
    is_visible = 0;
    int before = helpers;
    int status = pw_data_mount_request_with(&ops, 7, 0, &detail);
    int saved = errno;
    pw_startup_checkpoint_emit(sink, PW_CP_DATA_RETURN, status, detail.helper_errno);
    assert(status == -1 && detail.helper_errno == ETIMEDOUT && errno == saved);
    assert(helpers == before + 1 && uid == 1 && calls == 5);
    errno = EDOM; pw_startup_checkpoint_emit(NULL, PW_CP_DATA_RETURN, 999, 999);
    pw_startup_checkpoint_emit(sink, (PwStartupCheckpoint)999, 999, 999);
    assert(errno == EDOM && calls == 5);
    for (int i = 0; i < PW_CP_COUNT; i++) pw_startup_checkpoint_emit(sink, (PwStartupCheckpoint)i, 999, 999);
    return 0;
}
