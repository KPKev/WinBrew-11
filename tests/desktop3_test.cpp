// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main() {
  const char *file = "build/pointer-preferences-test.ini";
  std::remove(file);
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    s.activate(111);
    s.activate(142);
    s.activate(147);
    for (int i = 0; i < 20; ++i)
      s.activate(144);
    assert(s.pointer_speed == 10 && s.cursor_size == 2 && s.safe_area == 32 &&
           s.save_state == 1 && !s.dialog);
  }
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.dark && s.pointer_speed == 10 && s.cursor_size == 2 &&
           s.safe_area == 32 && s.save_state == 1);
    for (bool dark : {false, true}) {
      s.dark = dark;
      s.left_taskbar = false;
      s.focus = -1;
      s.hover = -1;
      s.pointer(400, 400, false);
      s.draw();
      constexpr int start_x = 697;
      constexpr int start_y = ww::H - 42;
      const auto background = s.pixels[(start_y + 7) * ww::W + start_x - 1];
      assert(s.pixels[start_y * ww::W + start_x] == background);
      assert(s.pixels[(start_y + 7) * ww::W + start_x + 14] == background);
      assert(s.pixels[(start_y + 14) * ww::W + start_x + 7] == background);
      const auto top_left = s.pixels[(start_y + 7) * ww::W + start_x + 7];
      const auto bottom_right = s.pixels[(start_y + 22) * ww::W + start_x + 22];
      assert(top_left != background && bottom_right != background);
      assert((top_left & 255) > (bottom_right & 255));
      assert(((top_left >> 8) & 255) > ((bottom_right >> 8) & 255));
    }
    s.dark = true;
    for (int size = 0; size < 3; ++size)
      for (int safe : {0, 16, 32}) {
        s.cursor_size = size;
        s.safe_area = safe;
        for (int x : {-100, 0, 1919, 2000})
          for (int y : {-100, 0, 1079, 2000}) {
            s.pointer(x, y, false);
            auto r = s.cursor_bounds(s.mx, s.my);
            assert(r.x >= safe && r.y >= safe && r.x + r.w <= ww::W - safe &&
                   r.y + r.h <= ww::H - safe);
            s.draw();
          }
      }
    s.safe_area = 0;
    s.cursor_size = 0;
    s.pointer(400, 400, false);
    s.pointer_speed = 1;
    for (int i = 0; i < 30; ++i)
      s.move_stick(255, 128, 1.f / 60);
    int slow = s.mx - 400;
    s.pointer(400, 400, false);
    s.pointer_speed = 10;
    for (int i = 0; i < 30; ++i)
      s.move_stick(255, 128, 1.f / 60);
    assert(s.mx - 400 > slow * 3);
    int old = s.mx;
    for (int i = 0; i < 30; ++i)
      s.move_stick(140, 128, 1.f / 60);
    assert(s.mx == old);
    s.activate(150);
    assert(s.quick);
    s.draw();
    s.key(ww::Key::back);
    assert(!s.quick);
    s.activate(151);
    assert(s.tray);
    s.draw();
    s.key(ww::Key::back);
    assert(!s.tray);
    std::snprintf(s.preferences, sizeof(s.preferences),
                  "build/no-such-wine11-dir/settings.ini");
    s.dialog = false;
    s.activate(110);
    assert(!s.dark && s.dialog && s.save_state == -1 && s.save_error != 0);
    assert(std::strstr(s.message, "session"));
  }
  {
    FILE *f = std::fopen(file, "wb");
    assert(f);
    std::fputs("1 1 1 0 1 1 0 2 1\n", f);
    std::fclose(f);
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.dark && s.clock24 && s.cursor_size == 2 && s.pointer_speed == 5 &&
           s.accent == 2);
    assert(s.save_preferences());
  }
  std::remove(file);
  std::puts("PASS: verified preferences restart/migration/failure, full cursor "
            "at screen edges, speed and deadzone, tray dismissal");
}
