// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include "paint_test_helpers.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
int main() {
  std::filesystem::create_directories("build/paint-input-output");
  ww::Shell s;
  assert(s.init("assets"));
  std::snprintf(s.preferences, sizeof(s.preferences),
                "build/paint-input-output/settings.ini");
  s.paint.show(s);
  s.draw();
  auto r = s.paint.canvas_rect({s.wx, s.wy, 1200, 780});
  const int x = r.x + r.w / 2, y = r.y + r.h / 2;
  s.input_device = ww::InputDevice::mouse;
  s.pointer(x, y, true, true);
  s.pointer(x + 25, y, false, true);
  s.pointer(x + 25, y, false, false);
  assert(s.paint.dirty());
  s.activate(33);
  assert(s.window == 5 && s.paint.modal());
  s.activate(12);
  assert(s.pending == ww::Action::none);
  s.key(ww::Key::back);
  assert(!s.paint.modal() && s.paint.dirty());
  s.activate(12);
  assert(s.paint.modal() && s.pending == ww::Action::none);
  s.activate(740);
  finish_paint_save(s.paint, s);
  assert(!s.paint.modal() && !s.paint.dirty());
  assert(s.pending == ww::Action::playden);
  bool saved_copy = false;
  for (const auto &entry :
       std::filesystem::directory_iterator("build/paint-input-output"))
    if (entry.path().extension() == ".png")
      saved_copy = true;
  assert(saved_copy);
  s.pending = ww::Action::none;
  s.paint.show(s);
  s.paint.stroke(10, 10, true);
  s.activate(91);
  assert(s.window == 0);
  s.key(ww::Key::start);
  assert(s.window == 5 && s.paint.modal() && !s.start);
  s.activate(742);
  s.activate(93);
  assert(s.paint.modal());
  s.activate(741);
  assert(s.window == 0 && !s.paint.dirty());
  s.paint.show(s);
  s.paint.stroke(10, 10, true);
  std::snprintf(s.preferences, sizeof(s.preferences),
                "build/no-such-paint-input-output/settings.ini");
  s.activate(93);
  s.activate(740);
  finish_paint_save(s.paint, s);
  assert(s.window == 5 && s.paint.modal() && s.paint.dirty());
  s.activate(742);
  s.paint.open(s, "tests/fixtures/paint-alpha.png");
  assert(s.paint.modal());
  s.activate(741);
  assert(!s.paint.modal() && !s.paint.dirty() && s.paint.width == 7);
  s.paint.blank();
  s.paint.show(s);
  s.draw();
  s.input_device = ww::InputDevice::controller;
  s.pad_buttons = 0x4000;
  s.focus = 730;
  s.key(ww::Key::accept);
  assert(s.paint.painting && s.paint.dirty());
  s.pad_buttons = 0;
  s.draw();
  assert(!s.paint.painting);
  auto revision = s.paint.revision;
  s.start = true;
  s.pointer(x, y, true, true);
  assert(s.paint.revision == revision);
  std::puts("PASS: integrated Paint mouse/controller canvas, "
            "switch/launch/Start guards, minimized changes, "
            "cancel/discard/save replay and failed-save modal retention");
}
