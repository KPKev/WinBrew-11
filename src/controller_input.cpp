// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
namespace ww {
void Shell::controller_input(bool connected, uint32_t buttons, int lx, int ly,
                             int rx, int ry, float seconds) {
  pad_connected = connected;
  if (!connected || (buttons & 0x80000000u)) {
    pad_buttons = 0;
    pad_lx = pad_ly = pad_rx = pad_ry = 128;
    if (drag_device == InputDevice::controller)
      drag = false;
    return;
  }
  uint32_t edge = buttons & ~pad_buttons;
  pad_buttons = buttons;
  pad_lx = lx;
  pad_ly = ly;
  pad_rx = rx;
  pad_ry = ry;
  if (window == 2 && settings_page == 8) {
    if (edge & 8) {
      settings_page = 7;
      focus = 107;
    }
    return;
  }
  if (edge)
    input_device = InputDevice::controller;
  if ((edge & 0x2000u) && pad_touch_drag_locked) {
    finish_touchpad_drag();
    return;
  }
  const bool vapor_open = window == 1 && file_games;
  const bool modal_open = dialog || notepad_modal() || paint.modal() || touch_keyboard || context ||
      file_context || file_rename_prompt || file_properties || files_busy();
  // Physical Circle and Square are Vapor navigation shortcuts, independent of
  // the desktop's optional confirm-button swap. Resolve an open modal first.
  if (vapor_open && (edge & 0x2000u)) {
    if (modal_open) key(Key::back);
    else { if (text_target == TextTarget::vapor_search) end_text(); library_downloads = false; steam_panel = false; steam_selection = -1; focus = -1; }
    return;
  }
  if (vapor_open && !modal_open && (edge & 0x8000u)) {
    if (text_target == TextTarget::vapor_search) end_text();
    library_downloads = true; steam_panel = false; focus = -1;
    return;
  }
  struct Binding {
    uint32_t bit;
    Key key;
  };
  const Binding keys[] = {{8, Key::start},
                          {0x10, Key::up},
                          {0x20, Key::right},
                          {0x40, Key::down},
                          {0x80, Key::left},
                          {0x1000, Key::library}};
  for (auto b : keys)
    if (edge & b.bit)
      key(b.key);
  const bool steam_list = steam && window == 1 && file_games && !steam_panel &&
      ((library_downloads && steam_queue_bounds.w > 0) || (!library_downloads && steam_list_bounds.w > 0)) &&
      !modal_open && !start && !calendar && !quick && !tray;
  const bool explorer_list=window==1&&!file_games&&!dialog&&!notepad_modal()&&!file_context&&!file_rename_prompt&&!file_properties&&!files_busy()&&!touch_keyboard&&!start&&!calendar&&!quick&&!tray;
  if(explorer_list){move_stick(lx,ly,seconds);const int amount=ry-128,magnitude=amount<0?-amount:amount;if(magnitude>pad_deadzone){float dt=seconds<0?0:seconds>.05f?.05f:seconds;steam_scroll_fraction+=(amount<0?-1.f:1.f)*float(magnitude-pad_deadzone)/float(128-pad_deadzone)*16.f*dt;int rows=int(steam_scroll_fraction);if(rows){if(file_nav_bounds.has(mx,my))file_nav_scroll_by(rows);else file_scroll_by(rows);steam_scroll_fraction-=rows;}}else steam_scroll_fraction=0;
  }else if (steam_list) {
    // Reserve the right stick for the game list in the library; the left
    // stick continues to move the pointer while this view is open.
    move_stick(lx, ly, seconds);
    const int amount = ry - 128;
    const int magnitude = amount < 0 ? -amount : amount;
    if (magnitude > pad_deadzone) {
      float dt = seconds < 0 ? 0 : seconds > .05f ? .05f : seconds;
      const float strength = float(magnitude - pad_deadzone) / float(128 - pad_deadzone);
      steam_scroll_fraction += (amount < 0 ? -1.f : 1.f) * strength * 14.f * dt;
      const int rows = int(steam_scroll_fraction);
      if (rows) {
        if (library_downloads) scroll_steam_queue(rows);
        else if (steam_home_bounds.has(mx, my)) scroll_steam_home(rows);
        else scroll_steam_library(rows);
        steam_scroll_fraction -= rows;
      }
    } else steam_scroll_fraction = 0;
  } else {
    steam_scroll_fraction = 0;
    move_stick(pad_right_stick ? rx : lx, pad_right_stick ? ry : ly, seconds);
  }
  uint32_t confirm = pad_swap_confirm ? 0x2000u : 0x4000u;
  uint32_t secondary = pad_swap_confirm ? 0x4000u : 0x2000u;
  if (edge & secondary) {
    if (dialog || notepad_modal() || paint.modal() || touch_keyboard || context ||
        file_context || file_rename_prompt || file_properties || files_busy())
      key(Key::back);
    else
      open_context();
  }
  if (edge & confirm) {
    if (focus >= 0)
      key(Key::accept);
    else
      pointer(mx, my, true, true, true);
  }
  if (!(buttons & (confirm | 0x100000u)) && !pad_touch_drag_locked && drag_device == InputDevice::controller)
    drag = false;
}
void Shell::controller_test_ui(Rect wr) {
  int cx = wr.x + 300, cw = wr.w - 332, y = wr.y;
  auto card = [&](Rect r) { round(r, 8, dark ? 0xff3b3330 : 0xffffffff); };
  card({cx, y + 142, cw, 132});
  text(cx + 22, y + 162,
       pad_connected ? "Controller connected" : "Waiting for controller input",
       1, ink());
  text(cx + 22, y + 205,
       "Move both sticks and press the face buttons or D-pad.", 0, muted());
  text(cx + 22, y + 240, "Options: return to controller settings", 0, muted());
  button({cx + cw - 202, y + 219, 180, 34}, 107, "< Controller settings");
  for (int stick = 0; stick < 2; ++stick) {
    int x = cx + stick * (cw / 2 + 10), width = cw / 2 - 10;
    card({x, y + 292, width, 250});
    text(x + 22, y + 311, stick ? "Right stick" : "Left stick", 1, ink());
    int px = x + width / 2, py = y + 430;
    round({px - 69, py - 69, 138, 138}, 69, dark ? 0xff4a4137 : 0xffeee8e2);
    line(px - 60, py, px + 60, py, muted());
    line(px, py - 60, px, py + 60, muted());
    int rawx = stick ? pad_rx : pad_lx, rawy = stick ? pad_ry : pad_ly;
    round({px + (rawx - 128) * 56 / 128 - 8, py + (rawy - 128) * 56 / 128 - 8,
           16, 16},
          8, accent_color());
  }
  const char *labels[] = {"Cross",      "Circle",     "Square",
                          "Triangle",   "D-pad Up",   "D-pad Down",
                          "D-pad Left", "D-pad Right"};
  const uint32_t masks[] = {0x4000, 0x2000, 0x8000, 0x1000,
                            0x10,   0x40,   0x80,   0x20};
  for (int i = 0; i < 8; ++i) {
    Rect r{cx + (i % 4) * (cw / 4 + 4), y + 562 + (i / 4) * 83, cw / 4 - 12,
           64};
    round(r, 7, (pad_buttons & masks[i]) ? accent_color() : surface());
    center(r.x, r.y + 22, r.w, labels[i], 0,
           (pad_buttons & masks[i]) ? 0xffffffff : ink());
  }
  text(cx + 22, y + 735, pad_touch_test_active ? (pad_touch_test_pressed ? "Touchpad: touching and pressed" : "Touchpad: finger detected")
                                              : (pad_touch_test_pressed ? "Touchpad: pressed" : "Touchpad: waiting for touch"),
       0, muted());
}
} // namespace ww
