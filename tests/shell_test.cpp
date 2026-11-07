// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  ww::Shell s;
  assert(s.init("assets"));
  s.draw();
  s.key(ww::Key::start);
  s.draw();
  assert(s.start);
  s.character('Q');
  s.character('U');
  assert(!std::strcmp(s.search, "qu"));
  s.draw();
  s.key(ww::Key::next);
  assert(s.focus == 30);
  s.key(ww::Key::accept);
  assert(s.pending == ww::Action::quake);
  assert(!s.start);
  s.pending = ww::Action::none;
  s.activate(31);
  assert(s.pending == ww::Action::playden);
  s.activate(32);
  s.draw();
  assert(s.window == 1);
  s.pointer(500, 170, true, true);
  s.pointer(550, 220, false, true);
  assert(s.wx == 410 && s.wy == 205);
  s.pointer(550, 220, false, false);
  s.activate(92);
  assert(s.maximized);
  s.activate(91);
  assert(s.window == 0);
  s.activate(32);
  assert(s.window == 1);
  s.activate(93);
  assert(s.window == 0 && s.restored == 0);
  s.maximized = false;
  s.draw();
  s.pointer(30, 220, true, false, false);
  assert(s.window == 0 && s.focus == 35);
  s.pointer(30, 220, true, false, true);
  assert(s.window == 1 && s.file_games);
  s.activate(93);
  s.activate(45);
  s.month = 12;
  s.year = 2026;
  s.activate(47);
  assert(s.month == 1 && s.year == 2027);
  s.activate(46);
  assert(s.month == 12 && s.year == 2026);
  s.result(false, 0x80940031, "launch");
  assert(s.dialog && std::strstr(s.message, "80940031"));
  s.key(ww::Key::back);
  assert(!s.dialog);
  s.activate(40);
  for (int i = 0; i < 110; i++)
    s.character('a');
  assert(std::strlen(s.search) == 79);
  s.key(ww::Key::erase);
  assert(std::strlen(s.search) == 78);
  std::puts("PASS: search, launch intents, window controls/drag, calendar "
            "rollover, error handling, input bounds");
}
