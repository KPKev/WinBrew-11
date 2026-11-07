// SPDX-License-Identifier: GPL-3.0-or-later
#include "brand_asset.hpp"
#include "shell.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace ww {
void Shell::brand_icon(int x, int y, int size, bool tile) {
  int source = tile ? 128 : 64;
  const auto *image = tile ? brand::tile : brand::mark;
  for (int py = 0; py < size; ++py)
    for (int px = 0; px < size; ++px)
      rect({x + px, y + py, 1, 1},
           image[(py * source / size) * source + px * source / size]);
}
Rect Shell::cursor_bounds(int x, int y) const {
  int height = cursor_size == 2 ? 36 : cursor_size == 1 ? 28 : 22;
  return {x, y, (height * 25 + 35) / 36 + 2, height + 2};
}
// Full arrow, closed outline and tail. Coordinates are the cursor hotspot.
void Shell::cursor(int x, int y, int height) {
  constexpr int vx[] = {1, 1, 8, 13, 18, 13, 24},
                vy[] = {1, 29, 23, 35, 33, 21, 21};
  auto inside = [&](int px, int py) {
    float sx = float(px) * 36 / height, sy = float(py) * 36 / height;
    bool in = false;
    for (int i = 0, j = 6; i < 7; j = i++)
      if ((vy[i] > sy) != (vy[j] > sy) &&
          sx < (vx[j] - vx[i]) * (sy - vy[i]) / float(vy[j] - vy[i]) + vx[i])
        in = !in;
    return in;
  };
  for (int py = 0; py < height; ++py)
    for (int px = 0; px < height * 25 / 36 + 1; ++px) {
      if (!inside(px, py))
        continue;
      bool edge = !inside(px - 1, py) || !inside(px + 1, py) ||
                  !inside(px, py - 1) || !inside(px, py + 1);
      rect({x + px, y + py, 1, 1}, edge ? 0xff201b17 : 0xffffffff);
    }
}
void Shell::move_stick(int lx, int ly, float seconds) {
  seconds = seconds < 0 ? 0 : seconds > .05f ? .05f : seconds;
  auto axis = [&](int raw) {
    int v = raw - 128;
    int a = std::abs(v);
    if (a <= pad_deadzone)
      return 0.f;
    float value = float(a - pad_deadzone) / (128 - pad_deadzone);
    return (v < 0 ? -1.f : 1.f) * value * value * (160 + pointer_speed * 90) *
           seconds;
  };
  pointer_fraction_x += axis(lx);
  pointer_fraction_y += axis(ly) * (pad_invert_y ? -1.f : 1.f);
  int dx = int(pointer_fraction_x), dy = int(pointer_fraction_y);
  pointer_fraction_x -= dx;
  pointer_fraction_y -= dy;
  if (dx || dy) {
    input_device = InputDevice::controller;
    focus = -1;
    pointer(mx + dx, my + dy, false, drag);
  }
}
void Shell::touchpad_focus() {
  for (int i = count - 1; i >= 0; --i) if (hits[i].r.has(mx, my)) {
    const int id = hits[i].id;
    focus = id == 89 || id == 90 ? -1 : id;
    return;
  }
  focus = -1;
}
void Shell::finish_touchpad_drag() {
  if (!pad_touch_drag_locked) return;
  pointer(mx, my, false, false); pad_touch_drag_locked = false;
  if (pad_touch_icon_key && !save_desktop_icon_positions()) {
    dialog = true; focus = 60;
    std::snprintf(message, sizeof(message), "Desktop position could not be saved; it will last for this session.");
  }
  pad_touch_icon_key = 0;
  if (pad_touch_pressed) pad_touch_suppress_release = true;
  pad_touch_pressed = false; pad_touch_hold_handled = true;
}
void Shell::touchpad_input(bool connected, unsigned fingers, int x, int y, int finger,
                           bool pressed, std::uint64_t timestamp_us) {
  if (!connected) {
    if (pad_touch_pressed || pad_touch_drag_locked) pointer(mx, my, false, false);
    pad_touch_active = pad_touch_pressed = pad_touch_drag_locked = false;
    pad_touch_test_active = pad_touch_test_pressed = pad_touch_suppress_release = false;
    pad_touch_hold_handled = pad_touch_moved = false; pad_touch_id = -1; pad_touch_icon_key = 0;
    pad_gesture_mode = 0; touchpad_fraction_x = touchpad_fraction_y = 0;
    return;
  }
  const bool valid = fingers == 1 && x >= 0 && x <= 1920 && y >= 0 && y <= 1080 && finger >= 0;
  if (window == 2 && settings_page == 8) {
    pad_touch_test_active = valid; pad_touch_test_pressed = pressed;
    pad_touch_suppress_release = pressed; pad_touch_active = pad_touch_pressed = false;
    pad_touch_id = -1; pad_touch_drag_locked = false; pad_touch_icon_key = 0;
    return;
  }
  if (pad_touch_suppress_release) {
    if (!pressed) { pad_touch_suppress_release = false; pad_touch_pressed = false; pad_touch_active = false; pad_touch_id = -1; pad_gesture_mode = 0; }
    return;
  }
  const bool new_contact = valid && !pad_touch_active;
  if (fingers > 2) pad_gesture_mode = 3;
  else if (fingers == 2 && pad_gesture_mode != 2 && pad_gesture_mode != 3) {
    pad_gesture_mode = 2; pad_gesture_start_us = timestamp_us; pad_gesture_x = x; pad_gesture_y = y;
    pad_gesture_moved = false; pad_gesture_physical = pressed;
  } else if (fingers == 1 && pad_gesture_mode == 0) {
    pad_gesture_mode = 1; pad_gesture_start_us = timestamp_us; pad_gesture_x = x; pad_gesture_y = y;
    pad_gesture_moved = false; pad_gesture_physical = pressed;
  }
  if (fingers && pad_gesture_mode && (std::abs(x - pad_gesture_x) > 12 || std::abs(y - pad_gesture_y) > 12))
    pad_gesture_moved = true;
  if (pressed) pad_gesture_physical = true;
  if (!fingers && pad_gesture_mode) {
    const bool tap = !pad_gesture_moved && !pad_gesture_physical && !pad_touch_drag_locked &&
        timestamp_us >= pad_gesture_start_us && timestamp_us - pad_gesture_start_us <= 250000;
    if (tap && pad_gesture_mode == 2) { input_device = InputDevice::controller; focus = -1; open_context(); }
    pad_gesture_mode = 0;
  }
  int dx = 0, dy = 0;
  if (valid && pad_gesture_mode != 2 && pad_gesture_mode != 3) {
    if (pad_touch_active && finger == pad_touch_id) {
      int raw_dx = x - pad_touch_x, raw_dy = y - pad_touch_y;
      // A lost sample or an unintended contact change must not fling the cursor.
      if (std::abs(raw_dx) > 300 || std::abs(raw_dy) > 300) {
        pad_touch_x = x; pad_touch_y = y; raw_dx = raw_dy = 0;
      }
      if (pressed) { pad_touch_x = x; pad_touch_y = y; raw_dx = raw_dy = 0; }
      auto deadzone = [this](int delta) {
        return delta > touchpad_deadzone ? delta - touchpad_deadzone
             : delta < -touchpad_deadzone ? delta + touchpad_deadzone : 0;
      };
      const int filtered_x = deadzone(raw_dx), filtered_y = deadzone(raw_dy);
      pad_touch_x += filtered_x; pad_touch_y += filtered_y;
      const auto dt = timestamp_us > pad_touch_motion_us ? timestamp_us - pad_touch_motion_us : 16000;
      const double milliseconds = std::max(1.0, std::min(100.0, double(dt) / 1000.0));
      const double velocity = std::sqrt(double(filtered_x * filtered_x + filtered_y * filtered_y)) / milliseconds;
      const double boost = 1.0 + double(touchpad_acceleration) * 0.12 * std::min(2.0, velocity);
      const double gain = (0.3 + double(touchpad_speed) * 0.14) * boost;
      touchpad_fraction_x += float(filtered_x * gain); touchpad_fraction_y += float(filtered_y * gain);
      dx = int(touchpad_fraction_x); dy = int(touchpad_fraction_y);
      touchpad_fraction_x -= dx; touchpad_fraction_y -= dy;
    } else {
      pad_touch_x = x; pad_touch_y = y; touchpad_fraction_x = touchpad_fraction_y = 0;
    }
    pad_touch_motion_us = timestamp_us; pad_touch_active = true; pad_touch_id = finger;
  } else { pad_touch_active = false; pad_touch_id = -1; touchpad_fraction_x = touchpad_fraction_y = 0; }
  const bool edge = pressed && !pad_touch_pressed;
  const bool released = !pressed && pad_touch_pressed;
  if (edge) {
    pad_touch_press_us = timestamp_us; pad_touch_press_x = mx + dx; pad_touch_press_y = my + dy;
    pad_touch_hold_handled = pad_touch_moved = false;
  }
  if (pressed && (std::abs(mx + dx - pad_touch_press_x) > 12 || std::abs(my + dy - pad_touch_press_y) > 12))
    pad_touch_moved = true;
  if (dx || dy || edge || released) input_device = InputDevice::controller;
  if (dx || dy) {
    focus = -1; pointer(mx + dx, my + dy, false, pad_touch_drag_locked && !pad_touch_icon_key);
    if (pad_touch_drag_locked && pad_touch_icon_key)
      set_desktop_icon_position(pad_touch_icon_key, mx - pad_touch_icon_dx, my - pad_touch_icon_dy);
    else if (!pad_touch_drag_locked && text_target == TextTarget::none) touchpad_focus();
  }
  else if (new_contact && !pad_touch_drag_locked && text_target == TextTarget::none) touchpad_focus();
  const bool long_press = (pressed || released) && !pad_touch_hold_handled && !pad_touch_moved &&
      timestamp_us >= pad_touch_press_us && timestamp_us - pad_touch_press_us >= std::uint64_t(touchpad_hold_ms) * 1000;
  if (long_press) {
    input_device = InputDevice::controller;
    pad_touch_hold_handled = true;
    if (pad_touch_drag_locked) {
      finish_touchpad_drag();
    } else {
      bool draggable = window == 1 && file_games && !steam_panel &&
          ((!library_downloads && (steam_scroll_track.has(mx, my) || steam_home_track.has(mx, my))) ||
           (library_downloads && steam_queue_track.has(mx, my)));
      for (int i = count - 1; !draggable && i >= 0; --i)
        if (hits[i].r.has(mx, my)) {
          const auto hit = hits[i]; const int id = hit.id;
          if (hit.r.w == 108 && hit.r.y < H - 54 &&
              (id == 10 || id == 11 || id == 13 || id == 34 || id == 35 ||
               (id >= 1200 && id < 1328 && rendered_shortcut_apps[id - 1200] &&
                (vapor_shortcuts.placements(rendered_shortcut_apps[id - 1200]) & VaporShortcuts::Desktop)))) {
            pad_touch_icon_key = id >= 1200 ? 0x100000000ULL | rendered_shortcut_apps[id - 1200] : std::uint64_t(id);
            pad_touch_icon_dx = mx - hit.r.x; pad_touch_icon_dy = my - hit.r.y;
            set_desktop_icon_position(pad_touch_icon_key, hit.r.x, hit.r.y);
            pad_touch_drag_locked = true;
          } else if (id >= 500 && id < 510 && window == 1 && !file_games) {
            dialog = true; focus = 60;
            std::snprintf(message, sizeof(message), "Moving files and folders by dragging is not available yet.");
          } else draggable = id == 90 && !maximized;
          break;
        }
      if (draggable) {
        pointer(mx, my, true, true);
        pad_touch_drag_locked = drag || steam_scroll_drag || steam_home_drag || steam_queue_drag;
      }
    }
  }
  if (released && !pad_touch_hold_handled && !pad_touch_moved && !pad_touch_drag_locked) {
    // One quick physical click opens a desktop icon or folder, as requested.
    pointer(mx, my, true, true, true); pointer(mx, my, false, false);
  }
  pad_touch_pressed = pressed;
}
} // namespace ww
