"""Apply Wine 11 return hooks only to the isolated installed-revision checkout.

No console requests or title/game/registry/ownership writes are performed.
"""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess

BASE = 'ce289425bb34acbcc98f77b4e00ac98b3c16e4bc'
MAIN_SHA = 'a8372d5097e84251ec762beeed3e0f521db596f3d6c2ea9d5c5863532b462def'
PROJECT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT / 'build/return-wrapper/source'


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Expected exactly one source anchor: ' + old[:80])
    return text.replace(old, new)


def apply():
    source = SOURCE.resolve()
    if not source.is_relative_to((PROJECT / 'build/return-wrapper').resolve()):
        raise RuntimeError('Isolated source path escaped the intended build directory')
    repository_root = subprocess.check_output(['git', '-C', str(source), 'rev-parse', '--show-toplevel'], text=True).strip()
    if Path(repository_root).resolve() != source:
        raise RuntimeError('Isolated launcher source is not a repository root')
    actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != BASE:
        raise RuntimeError('Isolated source is not the installed wrapper revision')
    if subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True):
        raise RuntimeError('Isolated source already has changes; inspect before applying')
    main = source / 'native/wine64_main.c'
    if hashlib.sha256(main.read_bytes()).hexdigest() != MAIN_SHA:
        raise RuntimeError('Exact installed-revision main source hash mismatch')
    text = main.read_text()
    text = once(text, '#include "../src/pw_title_session.h"',
                '#include "../src/pw_title_session.h"\n#include "return_intent.h"\n#include "return_native.h"')
    begin = text.index('static int per_game_argument(')
    end = text.index('\n/* ---- Wine stderr', begin)
    text = text[:begin] + '''static Wine11ReturnIntent wine11_return_intent;
static int per_game_argument(int argc, char **argv, char *error, size_t error_size)
{
    if (wine11_return_parse(argc, (const char *const *)argv, &wine11_return_intent) != 0) {
        snprintf(error, error_size, "invalid per-game session or Wine 11 return intent");
        return -1;
    }
    title_session_argument = wine11_return_intent.session_argument;
    return 0;
}
''' + text[end:]
    begin = text.index('static void on_exit_report(void)')
    end = text.index('\n/* ---- library', begin)
    text = text[:begin] + '''static int wine11_finish_session(void *unused, int was_running)
{
    (void)unused;
    return was_running ? pw_title_session_exited(&title_session, 0, 0)
                       : pw_title_session_failed(&title_session, 0, 0);
}
static void wine11_close_session(void *unused)
{
    (void)unused;
    pw_title_session_close(&title_session);
}
static void wine11_close_publication(void *unused)
{
    (void)unused;
    close_title_publication();
}
static uint32_t wine11_launch_return(void *unused, const char *title)
{
    uint32_t result = wine11_return_native_launch(unused, title);
    PS5LOG_LOG("PW_WINE64 return target=%s result=0x%08x", title, (unsigned)result);
    return result;
}
static void on_exit_report(void)
{
    Wine11ReturnExit route = WINE11_RETURN_HOME;
    PS5LOG_LOG("PW_WINE64 exit sink_calls=%lu", sink_calls);
    if (per_game_mode) {
        const Wine11ReturnExitOps ops = {
            NULL, wine11_finish_session, wine11_close_session,
            wine11_close_publication, wine11_launch_return
        };
        route = wine11_return_finish(&wine11_return_intent,
            __atomic_load_n(&per_game_running, __ATOMIC_ACQUIRE), &ops);
    } else {
        restart_title(NULL, launch.cycle + 1u, "wine-exit");
    }
    pw_diagnostics_close("wine64-exit");
    if (per_game_mode) {
        if (route == WINE11_RETURN_WAIT_FOR_HANDOFF) {
            /* The platform normally terminates us during accepted handoff.
             * A failed asynchronous request must not leave a hung game title. */
            for (int waited = 0; waited < 100; ++waited) usleep(100000);
        }
        leave_to_home();
    }
}
''' + text[end:]
    text = once(text, '    pw_hid_ps5_preload(hid_log);', '''    if (per_game_mode && wine11_return_intent.requested) {
        uint32_t result = wine11_return_native_preload();
        PS5LOG_LOG("PW_WINE64 return provider preload=0x%08x", (unsigned)result);
    }
    pw_hid_ps5_preload(hid_log);''')
    main.write_text(text, newline='\n')
    here = Path(__file__).resolve().parent
    for name in ['return_intent.c', 'return_intent.h', 'return_native.c', 'return_native.h']:
        shutil.copy2(here / name, source / 'native' / name)
    for name in ['return_intent_test.c', 'return_native_test.c']:
        target = 'test_wine11_' + name.removesuffix('_test.c') + '.c'
        shutil.copy2(here / name, source / 'tests' / target)
    script = source / 'tools/build_native.sh'
    build = script.read_text()
    build = once(build, '    native/wine64_main.c native/pw_diagnostics.c',
                 '    native/wine64_main.c native/return_intent.c native/return_native.c native/pw_diagnostics.c')
    build = once(build, 'destination.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\\n")', '''manifest["wine11_return"] = {
    "feature": "wine11-return-on-exit-v1",
    "argument": "wine11-return=PPSA77381",
    "target": "PPSA77381",
    "normal_exit_only": True,
    "base_source_revision": "ce289425bb34acbcc98f77b4e00ac98b3c16e4bc",
}
destination.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\\n")''')
    script.write_text(build, newline='\n')
    # Keep the public descriptor, session and publication parser sources intact.
    protected = ['src/pw_title_launch.c', 'src/pw_title_session.c',
                 'src/pw_title_publication.c', 'native/pw_title_publication_native.c']
    if subprocess.check_output(['git', '-C', str(source), 'diff', '--', *protected], text=True):
        raise RuntimeError('Protected descriptor/session/publication sources changed')
    print('Applied isolated opt-in return variant; protected protocol sources unchanged')


if __name__ == '__main__':
    argparse.ArgumentParser(description=__doc__).parse_args()
    apply()
