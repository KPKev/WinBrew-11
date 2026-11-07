// SPDX-License-Identifier: GPL-3.0-or-later
// PS5 report layouts checked against prospero-win/native/pw_hid_ps5
// (LGPL-2.1-or-later).
#pragma once
#include <cstdint>
namespace ww {
struct Shell;
struct KeyboardReport {
  bool connected = false;
  uint8_t modifiers = 0;
  uint32_t locks = 0;
  uint16_t keys[16]{};
  int count = 0;
};
KeyboardReport parse_keyboard(const uint8_t *state);
struct HidInput {
  uint16_t held[16]{};
  int held_count = 0;
  uint16_t repeating = 0;
  uint64_t next_repeat = 0, click_time = 0;
  uint32_t buttons = 0;
  int click_x = 0, click_y = 0;
  float mouse_fraction_x = 0, mouse_fraction_y = 0;
  void keyboard(Shell &, const uint8_t *state, uint64_t now_us);
  void mouse(Shell &, const uint8_t *records, int count, uint64_t now_us);
};
} // namespace ww
