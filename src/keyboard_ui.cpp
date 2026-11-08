// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
namespace ww {
void Shell::keyboard_ui() {
  if (!touch_keyboard || text_target == TextTarget::none)
    return;
  Rect panel_rect{460, 626, 1000, 388};
  panel(panel_rect);
  add(panel_rect, 89);
  text(487, 644, "Touch keyboard", 1, ink());
  text(680, 646,
       text_target == TextTarget::file_address ? "Folder address"
       : text_target == TextTarget::file_search ? "Search files"
       : text_target == TextTarget::file_rename ? "Rename item"
       : text_target == TextTarget::filename ? "File name"
       : text_target == TextTarget::document  ? "Notepad"
       : text_target == TextTarget::address ? "Web address"
       : text_target == TextTarget::vapor_search ? "Search games"
                                            : "Search apps",
       0, muted());
  button({1344, 638, 90, 34}, 246, "Hide");
  const char *normal[] = {"1234567890", "qwertyuiop", "asdfghjkl.",
                          "zxcvbnm-_/"};
  const char *symbols[] = {"1234567890", "!@#$%^&*()",
                           "-_=+[]{};:", "\\|,.<>/?'\""};
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 10; ++col) {
      Rect key{493 + col * 93, 686 + row * 61, 84, 51};
      int id = 200 + row * 10 + col;
      add(key, id);
      round({key.x, key.y + 2, key.w, key.h}, 6,
            dark ? 0xff101010 : 0xffc3c9d2);
      uint32_t fill = dark ? 0xff45413b : 0xffffffff;
      if (hover == id || focus == id)
        fill = accent_color();
      round(key, 6, fill);
      const char *keys = keyboard_symbols ? symbols[row] : normal[row];
      char c = keys[col];
      if (keyboard_shift && row == 0)
        c = "!@#$%^&*()"[col];
      if (keyboard_shift && c >= 'a' && c <= 'z')
        c -= 32;
      char label[2] = {c, 0};
      center(key.x, key.y + 15, key.w, label, 1,
             (hover == id || focus == id) ? 0xffffffff : ink());
    }
  struct Control {
    int x, w, id;
    const char *label;
  };
  const Control controls[] = {
      {493, 112, 240, keyboard_shift ? "Shift on" : "Shift"},
      {615, 112, 241, keyboard_symbols ? "ABC" : "&123"},
      {737, 264, 242, "Space"},
      {1011, 86, 244, "<"},
      {1107, 86, 245, ">"},
      {1203, 102, 243, "Erase"},
      {1315, 108, 248,
       text_target == TextTarget::document ? "New line" : "Enter"}};
  for (auto control : controls) {
    Rect key{control.x, 944, control.w, 47};
    round({key.x, key.y + 2, key.w, key.h}, 6, dark ? 0xff101010 : 0xffc3c9d2);
    round(key, 6, dark ? 0xff45413b : 0xfff6f4f1);
    button(key, control.id, control.label);
  }
}
} // namespace ww
