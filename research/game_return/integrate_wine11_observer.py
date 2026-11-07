"""Apply observer integration only to the separate hash-checked Wine11 copy."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / 'build/owner-observer-integration/project'
EXACT = ROOT / 'build/return-wrapper/source'
HERE = Path(__file__).resolve().parent


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Expected one integration anchor: ' + old[:70])
    return text.replace(old, new)


def edit(name, old, new):
    path = PROJECT / name
    path.write_text(once(path.read_text(), old, new), newline='\n')


def main():
    if not (PROJECT.parent / 'baseline/eboot.bin').is_file():
        raise RuntimeError('Build the isolated stock baseline first')
    module = PROJECT / 'src/owner_admission'
    if module.exists():
        raise RuntimeError('Integration already exists; inspect it before applying again')
    if hashlib.sha256((EXACT / 'native/pw_title_publication_native.c').read_bytes()).hexdigest() != '9a3841bf42b7e70f87f180630f9ccb48d2b4e64b98185c895b0e168b4e4f6e7c':
        raise RuntimeError('Exact installed native reader hash mismatch')
    module.mkdir()
    for folder in ['native', 'src', 'include']:
        (module / folder).mkdir()
    for name in ['admission_observer.c', 'admission_observer.h']:
        shutil.copy2(HERE / name, module / name)
    observer = module / 'admission_observer.c'
    observer.write_text(once(observer.read_text(),
        '#error Exact installed-revision publication reader source is required',
        '#define WINE11_PUBLICATION_NATIVE_SOURCE "native/pw_title_publication_native.inc"'), newline='\n')
    for name in ['pw_title_publication_native.h', 'pw_title_publication_ps5_lock.c', 'pw_title_publication_ps5_lock.h']:
        shutil.copy2(EXACT / 'native' / name, module / 'native' / name)
    shutil.copy2(EXACT / 'native/pw_title_publication_native.c', module / 'native/pw_title_publication_native.inc')
    names = ['pw_title_publication', 'pw_title_launch', 'pw_game_profile', 'pw_app_profile',
             'pw_profile_catalog', 'pw_wine_launch', 'pw_publication_sha256']
    for name in names:
        shutil.copy2(EXACT / 'src' / (name + '.c'), module / 'src' / (name + '.c'))
    for path in (EXACT / 'src').glob('*.h'):
        shutil.copy2(path, module / 'src' / path.name)
    for path in (EXACT / 'include').glob('*.h'):
        shutil.copy2(path, module / 'include' / path.name)
    shutil.copytree(EXACT / 'third_party/mbedtls-publication', module / 'third_party/mbedtls-publication')
    (module / 'forced_config.h').write_text('#ifndef MBEDTLS_CONFIG_FILE\n#define MBEDTLS_CONFIG_FILE "pw_publication_config.h"\n#endif\n', newline='\n')
    provenance = {'base_revision': 'ce289425bb34acbcc98f77b4e00ac98b3c16e4bc',
                  'read_only': True, 'files': {p.relative_to(module).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in module.rglob('*') if p.is_file()}}
    (module / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', newline='\n')
    edit('src/shell.hpp', '  Action pending = Action::none;',
         '  Action pending = Action::none;\n  bool owner_check_requested = false;\n  int owner_check_status = -1;')
    (PROJECT / 'src/owner_check_dispatch.hpp').write_text('''// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "shell.hpp"
#include "owner_admission/admission_observer.h"
namespace ww {
using OwnerObserver = Wine11AdmissionReport (*)(int);
inline void dispatch_owner_check(Shell &shell, bool granted, OwnerObserver observer) {
  if (!shell.owner_check_requested) return;
  shell.owner_check_requested = false;
  if (shell.pending != Action::none) { shell.owner_check_status = -1; return; }
  auto result = observer ? observer(granted ? 1 : 0) : Wine11AdmissionReport{};
  int status = int(result.status);
  shell.owner_check_status = status >= 0 && status <= 8 ? status : 4;
}
inline const char *owner_check_line(int status) {
  if (status == -1) return "Check Quake II RTX automatic return compatibility.";
  if (status == -2) return "Waiting to run the read-only check.";
  if (status == WINE11_ADMISSION_READ_ONLY_PASSED) return "Read-only checks passed. Return is not installed yet.";
  if (status == WINE11_ADMISSION_BUSY) return "The game is busy. Close it and try again.";
  if (status == WINE11_ADMISSION_NO_ACCESS) return "The required game details are not accessible.";
  if (status == WINE11_ADMISSION_WRONG_GAME) return "This installation does not match Quake II RTX.";
  return "The game details could not be verified.";
}
} // namespace ww
''', newline='\n')
    edit('src/shell.cpp', '  if (id == 965) {', '''  if (id == 185) {
    if (window == 2 && settings_page == 4 && !dialog && !owner_check_requested) {
      owner_check_requested = true;
      owner_check_status = -2;
    }
    return;
  }
  if (id == 965) {''')
    edit('src/desktop_ui.cpp', '#include "shell.hpp"', '#include "shell.hpp"\n#include "owner_check_dispatch.hpp"')
    edit('src/desktop_ui.cpp', '  } else if (settings_page == 8) {', '''    card(620, 126);
    text(cx + 22, y + 638, "Game return", 1, ink());
    button({cx + cw - 176, y + 635, 154, 38}, 185, "Run check");
    text(cx + 22, y + 681, owner_check_line(owner_check_status), 0, ink());
    text(cx + 22, y + 715, "Checks only. Your game files and settings stay unchanged.", 0, muted());
  } else if (settings_page == 8) {''')
    edit('src/ps5.cpp', '#include "shell.hpp"', '#include "shell.hpp"\n#include "owner_check_dispatch.hpp"')
    edit('src/ps5.cpp', '  native_input::poll(shell, now);',
         '  native_input::poll(shell, now);\n  ww::dispatch_owner_check(shell, elevated, wine11_admission_observe);')
    edit('src/host.cpp', '#include "shell.hpp"', '#include "shell.hpp"\n#include "owner_check_dispatch.hpp"')
    edit('src/host.cpp', '  if (shell.pending == ww::Action::browser) {',
         '  ww::dispatch_owner_check(shell, false, nullptr);\n  if (shell.pending == ww::Action::browser) {')
    edit('src/host.cpp', '    if (!std::strcmp(argv[1], "about")) {shell.window=2;shell.settings_page=4;}', '''    if (!std::strcmp(argv[1], "about")) {shell.window=2;shell.settings_page=4;}
    if (!std::strcmp(argv[1], "owner-check-pass")) {shell.window=2;shell.settings_page=4;shell.owner_check_status=8;}
    if (!std::strcmp(argv[1], "owner-check-busy")) {shell.window=2;shell.settings_page=4;shell.owner_check_status=3;}''')
    edit('tools/stage_build.py', "'title_launch.hpp','ps5.cpp']", "'title_launch.hpp','owner_check_dispatch.hpp','ps5.cpp']")
    edit('tools/stage_build.py', '# The VideoOut setup',
         "shutil.copytree(r/'src/owner_admission',stage/'src/owner_admission',dirs_exist_ok=True)\n# The VideoOut setup")
    edit('tools/prepare_native_imports.py', "(r/'build/native-build.sh').write_text(source,newline='\\n')", '''anchor='    args+=("${pacbrew_cflags[@]}")'
assert source.count(anchor)==1
source=source.replace(anchor,'    args+=(-include "$root/wine11/src/owner_admission/forced_config.h")\\n'+anchor)
(r/'build/native-build.sh').write_text(source,newline='\\n')''')
    shutil.copy2(EXACT / 'third_party/mbedtls-publication/LICENSE', PROJECT / 'licenses/MbedTLS-Publication-LICENSE.txt')
    notice = PROJECT / 'THIRD_PARTY_NOTICES.md'
    notice.write_text(notice.read_text() + '\nThe read-only owner diagnostic reuses prospero-win publication admission from installed revision ce289425 (LGPL-2.1-or-later), with its pinned Mbed TLS publication SHA-256 provider. Corresponding source and provenance are retained under src/owner_admission; the original provider license accompanies this preview. It performs no recovery or game-state writes.\n', newline='\n')
    print('Integrated user-invoked observer into the separate Wine11 snapshot only')


if __name__ == '__main__':
    main()
