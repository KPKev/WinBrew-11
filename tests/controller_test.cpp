// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
#ifdef _WIN32
  const char *file = "build/controller-preferences.ini";
#else
  const char *file = "/tmp/wine11-controller-preferences.ini";
#endif
  FILE *f = std::fopen(file, "wb");
  assert(f);
  std::fputs("3 1 1 0 1 0 0 2 1 7 1 16 60 1 9\n", f);
  std::fclose(f);
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.dark && s.pointer_speed == 7 && s.mouse_speed == 9 &&
           s.pad_deadzone == 22 && !s.pad_right_stick);
    s.activate(107);
    assert(s.settings_page == 7 && s.window == 2);
    s.draw();
    assert(s.count < 128);
    for (int i = 0; i < 7; i++) {
      s.activate(301);
      s.activate(302);
    }
    assert(s.pointer_speed == 10 && s.pad_deadzone == 10);
    s.activate(305);
    s.activate(306);
    s.activate(307);
    assert(s.pad_right_stick && s.pad_invert_y && s.pad_swap_confirm &&
           !s.dialog);
  }
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.dark && s.mouse_speed == 9 && s.pointer_speed == 10 &&
           s.pad_deadzone == 10 && s.pad_right_stick && s.pad_invert_y &&
           s.pad_swap_confirm);
    s.pointer(800, 600, false);
    s.controller_input(true, 0, 255, 128, 128, 128, .016f);
    assert(s.mx == 800);
    s.controller_input(true, 0, 128, 128, 255, 128, .016f);
    assert(s.mx > 800);
    s.controller_input(true, 0, 128, 128, 128, 255, .016f);
    assert(s.my < 600);
    s.pad_deadzone = 40;
    int old = s.mx;
    for (int i = 0; i < 60; i++)
      s.controller_input(true, 0, 128, 128, 160, 128, .016f);
    assert(s.mx == old);
    s.pad_swap_confirm = false;
    s.pad_right_stick = s.pad_invert_y = false;
    s.focus = 33;
    s.controller_input(true, 0x4000, 128, 128, 128, 128, 0);
    assert(s.window == 2);
    s.controller_input(true, 0, 128, 128, 128, 128, 0);
    s.pad_swap_confirm = true;
    s.controller_input(true, 0x4000, 128, 128, 128, 128, 0);
    assert(s.window == 2 && s.context);
    s.controller_input(true, 0, 128, 128, 128, 128, 0);
    s.context = false;
    s.focus = 33;
    s.controller_input(true, 0x2000, 128, 128, 128, 128, 0);
    assert(s.window == 2);
    s.controller_input(true, 0, 128, 128, 128, 128, 0);
    // The live test never launches a game or activates its underlying controls.
    s.activate(107);
    s.activate(309);
    s.draw();
    assert(s.settings_page == 8);
    s.controller_input(true, 0x1000 | 0x4000, 170, 128, 128, 90, .016f);
    assert(s.pad_lx == 170 && s.pad_ry == 90 &&
           s.pad_buttons == (0x1000 | 0x4000));
    assert(s.settings_page == 8 && s.pending == ww::Action::none && !s.start);
    s.touchpad_input(true, 1, 250, 200, 1, true, 1000000);
    assert(s.pad_touch_test_active && s.pad_touch_test_pressed && !s.pad_touch_drag_locked);
    s.controller_input(true, 8, 128, 128, 128, 128, 0);
    assert(s.settings_page == 7);
    s.touchpad_input(true, 1, 250, 200, 1, false, 1100000);
    assert(!s.pad_touch_pressed && !s.start); // Leaving the test cannot emit a click.
    s.controller_input(true, 0x80004000u, 255, 255, 0, 0, .016f);
    assert(s.pad_buttons == 0 && s.pad_lx == 128);
    s.drag = true;
    s.drag_device = ww::InputDevice::mouse;
    s.controller_input(false, 0, 0, 0, 0, 0, 0);
    assert(!s.pad_connected && s.drag);
    s.drag_device = ww::InputDevice::controller;
    s.controller_input(false, 0, 0, 0, 0, 0, 0);
    assert(!s.drag);
    s.activate(107);
    s.draw();
    ww::Rect toggle{};
    for (int i = 0; i < s.count; i++)
      if (s.hits[i].id == 306)
        toggle = s.hits[i].r;
    bool previous = s.pad_invert_y;
    s.pointer(toggle.x + 5, toggle.y + 5, true);
    assert(s.pad_invert_y != previous);
    s.activate(308);
    assert(s.pointer_speed == 5 && s.pad_deadzone == 22 && !s.pad_invert_y &&
           !s.pad_right_stick && !s.pad_swap_confirm);
    assert(s.dark && s.mouse_speed == 9);
    s.activate(329); assert(s.settings_page == 9);
    s.activate(321); s.activate(323); s.activate(325); s.activate(327);
    assert(s.touchpad_deadzone == 5 && s.touchpad_speed == 6 && s.touchpad_acceleration == 6 && s.touchpad_hold_ms == 700);
    s.draw();
    assert(s.count < 128);
  }
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.touchpad_deadzone == 5 && s.touchpad_speed == 6 && s.touchpad_acceleration == 6 && s.touchpad_hold_ms == 700);
  }
  {
    ww::Shell slow, fast;
    slow.touchpad_deadzone = fast.touchpad_deadzone = 4;
    slow.touchpad_acceleration = fast.touchpad_acceleration = 10;
    slow.touchpad_input(true, 1, 100, 100, 1, false, 1000000);
    fast.touchpad_input(true, 1, 100, 100, 1, false, 1000000);
    slow.touchpad_input(true, 1, 110, 100, 1, false, 1016000);
    fast.touchpad_input(true, 1, 110, 100, 1, false, 1004000);
    assert(fast.mx > slow.mx); // Acceleration depends on movement speed.
  }
  std::remove(file);
  std::puts("PASS: PS5 controller settings migration/restart, speed/deadzone "
            "bounds, selected/inverted stick, confirm/back mapping, passive "
            "live test and mixed-device drag");
}
