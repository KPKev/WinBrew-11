#include "../src/owner_check_dispatch.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
static int calls = 0, grant_seen = -1;
static Wine11AdmissionStatus next_status = WINE11_ADMISSION_READ_ONLY_PASSED;
static Wine11AdmissionReport observe(int grant) {
  ++calls; grant_seen = grant;
  return Wine11AdmissionReport{next_status, 15u, 0, 0, 0};
}
int main() {
  ww::Shell shell;
  assert(shell.init("assets", nullptr, nullptr, nullptr, "build/owner-check-ui-prefs.ini"));
  assert(!shell.owner_check_requested && shell.owner_check_status == -1);
  shell.activate(185);
  assert(!shell.owner_check_requested);
  shell.window = 2; shell.settings_page = 4;
  shell.draw(); shell.tick();
  assert(!calls && !shell.owner_check_requested);
  bool visible = false;
  for (int i = 0; i < shell.count; ++i) if (shell.hits[i].id == 185) visible = true;
  assert(visible);
  shell.activate(185);
  assert(shell.owner_check_requested && shell.pending == ww::Action::none);
  assert(shell.owner_check_status == -2 && !calls);
  ww::dispatch_owner_check(shell, true, observe);
  assert(calls == 1 && grant_seen == 1 && !shell.owner_check_requested);
  assert(shell.owner_check_status == 8 && shell.pending == ww::Action::none);
  ww::dispatch_owner_check(shell, true, observe);
  shell.draw(); shell.tick();
  assert(calls == 1);
  assert(!std::strcmp(ww::owner_check_line(8), "Read-only checks passed. Return is not installed yet."));
  assert(!std::strstr(ww::owner_check_line(8), "private"));
  const ww::Action actions[] = {ww::Action::quake, ww::Action::playden, ww::Action::browser};
  for (auto action : actions) {
    shell.pending = action; shell.owner_check_requested = true;
    ww::dispatch_owner_check(shell, true, observe);
    assert(calls == 1 && shell.pending == action && !shell.owner_check_requested);
  }
  shell.pending = ww::Action::none;
  shell.dialog = true; shell.activate(185); assert(!shell.owner_check_requested);
  shell.dialog = false; shell.settings_page = 0; shell.activate(185); assert(!shell.owner_check_requested);
  shell.settings_page = 4; shell.activate(185);
  next_status = static_cast<Wine11AdmissionStatus>(999);
  ww::dispatch_owner_check(shell, false, observe);
  assert(calls == 2 && grant_seen == 0 && shell.owner_check_status == 4);
  assert(!std::strcmp(ww::owner_check_line(shell.owner_check_status), "The game details could not be verified."));
  shell.activate(185); ww::dispatch_owner_check(shell, false, nullptr);
  assert(shell.owner_check_status == 0 && calls == 2);
  assert(shell.pending == ww::Action::none);
  puts("PASS: explicit About control only, no startup/draw polling, one-shot dispatch, modal gating, fixed messages, launch/browser priority preserved");
}
