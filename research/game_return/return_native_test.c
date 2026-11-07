/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "return_native.h"
#include "return_intent.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int queries, launches, user_error;
static uint32_t launch_result;
int sceSysmoduleLoadModuleInternal(uint32_t id) { assert(id == 0x80000010u); return 0; }
int sceUserServiceInitialize(const void *arg) { assert(!arg); return 0; }
int sceUserServiceGetForegroundUser(uint32_t *user) { ++queries; *user = 0x10000000u; return user_error; }
uint32_t sceSystemServiceLaunchApp(const char *title, const char **args, void *raw) {
    struct Params { uint32_t size, user, options; uint64_t crash; uint32_t flags; };
    const struct Params *p = raw;
    assert(!strcmp(title, WINE11_RETURN_TITLE) && !args);
    assert(sizeof(*p) == 32 && p->size == 32 && p->user == 0x10000000u);
    assert(!p->options && !p->crash && p->flags == 1);
    ++launches;
    return launch_result;
}
int main(void) {
    assert(wine11_return_native_launch(NULL, WINE11_RETURN_TITLE) == 0x80020026u);
    assert(!wine11_return_native_preload() && queries == 1);
    assert(wine11_return_native_launch(NULL, "FAKE78003") == 0x80020016u && !launches);
    assert(!wine11_return_native_launch(NULL, WINE11_RETURN_TITLE) && launches == 1 && queries == 2);
    launch_result = 0x80940001u;
    assert(wine11_return_native_launch(NULL, WINE11_RETURN_TITLE) == launch_result);
    user_error = (int)0x80960001u;
    assert(wine11_return_native_launch(NULL, WINE11_RETURN_TITLE) == (uint32_t)user_error && launches == 2);
    assert(wine11_return_native_preload() == (uint32_t)user_error);
    assert(wine11_return_native_launch(NULL, WINE11_RETURN_TITLE) == 0x80020026u && launches == 2);
    puts("PASS: native return provider preload, fixed-target/user validation, exact launch ABI and failure propagation");
}
