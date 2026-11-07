/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "return_intent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Test {
    char sequence[32];
    int report_error, was_running;
    uint32_t launch_result;
} Test;
static void step(Test *test, char value) {
    size_t count = strlen(test->sequence);
    test->sequence[count] = value;
    test->sequence[count + 1] = 0;
}
static int finish(void *context, int running) {
    Test *test = context;
    step(test, 'R'); test->was_running = running; return test->report_error;
}
static void close_session(void *context) { step(context, 'S'); }
static void close_publication(void *context) { step(context, 'P'); }
static uint32_t launch(void *context, const char *title) {
    assert(!strcmp(title, WINE11_RETURN_TITLE));
    Test *test = context;
    assert(!strcmp(test->sequence, "RSP"));
    step(test, 'L'); return test->launch_result;
}
int main(void) {
    Wine11ReturnIntent intent = {0};
    const char *session = "session=01234567-89ab-cdef-0123-456789abcdef";
    const char *legacy[] = {session};
    const char *opt_in[] = {WINE11_RETURN_ARGUMENT};
    const char *both[] = {session, WINE11_RETURN_ARGUMENT};
    assert(!wine11_return_parse(0, NULL, &intent) && !intent.requested && !intent.session_argument);
    assert(!wine11_return_parse(1, legacy, &intent) && !intent.requested && intent.session_argument == session);
    assert(!wine11_return_parse(2, both, &intent) && intent.requested && intent.session_argument == session);
    const char *duplicates[] = {WINE11_RETURN_ARGUMENT, WINE11_RETURN_ARGUMENT};
    Wine11ReturnIntent before = intent;
    assert(wine11_return_parse(2, duplicates, &intent) && !memcmp(&intent, &before, sizeof(intent)));
    const char *bad[] = {"wine11-return=PPSA77380", "wine11-return=FAKE78003", "wine11-return=/app0/eboot.bin",
        "session=01234567-89AB-cdef-0123-456789abcdef", "session=01234567-89ab-cdef-0123-456789abcdefEXTRA", "--quit"};
    for (size_t n = 0; n < sizeof(bad) / sizeof(bad[0]); ++n)
        assert(wine11_return_parse(1, &bad[n], &intent));
    char long_argument[70]; memset(long_argument, 'x', sizeof(long_argument)); long_argument[69] = 0;
    const char *long_argv[] = {long_argument};
    assert(wine11_return_parse(1, long_argv, &intent));
    assert(wine11_return_parse(1, NULL, &intent));

    Test test = {0};
    Wine11ReturnExitOps ops = {&test, finish, close_session, close_publication, launch};
    /* Direct Home launches and legacy Playden session launches still go Home. */
    assert(!wine11_return_parse(0, NULL, &intent));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_HOME);
    assert(!strcmp(test.sequence, "RSP"));
    test = (Test){0};
    assert(!wine11_return_parse(1, legacy, &intent));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_HOME);
    assert(!strcmp(test.sequence, "RSP"));
    test = (Test){0};
    assert(!wine11_return_parse(1, opt_in, &intent));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_WAIT_FOR_HANDOFF);
    assert(!strcmp(test.sequence, "RSPL"));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_HOME);
    assert(!strcmp(test.sequence, "RSPL"));
    test = (Test){0};
    assert(!wine11_return_parse(1, opt_in, &intent));
    assert(wine11_return_finish(&intent, 0, &ops) == WINE11_RETURN_HOME);
    assert(!test.was_running && !strcmp(test.sequence, "RSP"));
    test = (Test){.report_error = -1};
    assert(!wine11_return_parse(1, opt_in, &intent));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_HOME);
    assert(!strcmp(test.sequence, "RSP"));
    test = (Test){.launch_result = 0x80940001u};
    assert(!wine11_return_parse(1, opt_in, &intent));
    assert(wine11_return_finish(&intent, 1, &ops) == WINE11_RETURN_HOME);
    assert(!strcmp(test.sequence, "RSPL"));
    puts("PASS: return intent is opt-in, Home/session launches unchanged, cleanup precedes one fixed-target launch, failures fall back Home");
}
