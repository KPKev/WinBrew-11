/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "return_native.h"
#include "return_intent.h"
#include <stddef.h>
#include <string.h>
/* Same platform ABI used by Wine 11 and Playden's native title backend. */
struct Wine11LaunchParameters {
    uint32_t size, user, options;
    uint64_t crash_report;
    uint32_t check_flag;
};
_Static_assert(sizeof(struct Wine11LaunchParameters) == 32, "native launch ABI size");
_Static_assert(offsetof(struct Wine11LaunchParameters, check_flag) == 24, "native launch ABI offset");
int sceSysmoduleLoadModuleInternal(uint32_t);
int sceUserServiceInitialize(const void *);
int sceUserServiceGetForegroundUser(uint32_t *);
uint32_t sceSystemServiceLaunchApp(const char *, const char **, void *);
static int ready;
uint32_t wine11_return_native_preload(void)
{
    uint32_t user = 0;
    ready = 0;
    /* The wrapper already imports SystemService for LoadExec. Duplicate module
     * load / user initialization may report already-loaded; the user query is
     * the usable-provider test. Runtime dlsym is deliberately avoided. */
    (void)sceSysmoduleLoadModuleInternal(0x80000010u);
    (void)sceUserServiceInitialize(NULL);
    uint32_t result = (uint32_t)sceUserServiceGetForegroundUser(&user);
    ready = result == 0;
    return result;
}
uint32_t wine11_return_native_launch(void *unused, const char *title)
{
    uint32_t user = 0;
    (void)unused;
    if (!ready) return 0x80020026u; /* ENOSYS */
    if (!title || strcmp(title, WINE11_RETURN_TITLE)) return 0x80020016u; /* EINVAL */
    uint32_t result = (uint32_t)sceUserServiceGetForegroundUser(&user);
    if (result) return result | 0x80000000u;
    /* SkipLaunchCheck is the existing big-app handoff ABI after root grant.
     * This function is reached only after running/publication/session admission. */
    struct Wine11LaunchParameters parameters = {sizeof(parameters), user, 0, 0, 1};
    return sceSystemServiceLaunchApp(WINE11_RETURN_TITLE, NULL, &parameters);
}
