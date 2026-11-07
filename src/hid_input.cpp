// SPDX-License-Identifier: GPL-3.0-or-later
#include "hid_input.hpp"
#include "shell.hpp"
#include <cstdlib>
#include <cstring>
namespace ww {
template <class T> static T value(const uint8_t *data) {
  T v{};
  std::memcpy(&v, data, sizeof(v));
  return v;
}
KeyboardReport parse_keyboard(const uint8_t *state) {
  KeyboardReport report;
  if (!state || !state[16])
    return report;
  report.connected = true;
  report.modifiers = uint8_t(value<uint32_t>(state + 28));
  report.locks = value<uint32_t>(state + 24);
  int count = value<int32_t>(state + 20);
  count = count < 0 ? 0 : count > 16 ? 16 : count;
  for (int i = 0; i < count; ++i) {
    auto key = value<uint16_t>(state + 32 + i * 2);
    if (key > 3 && key < 0xe8)
      report.keys[report.count++] = key;
  }
  return report;
}
void HidInput::keyboard(Shell &shell, const uint8_t *state, uint64_t now) {
  auto report = parse_keyboard(state);
  shell.keyboard_connected = report.connected;
  bool fresh = false;
  for (int i = 0; i < report.count; ++i) {
    bool existed = false;
    for (int j = 0; j < held_count; ++j)
      if (held[j] == report.keys[i])
        existed = true;
    if (!existed) {
      auto key = report.keys[i];
      shell.physical_key(key, report.modifiers, report.locks);
      fresh = true;
      repeating = (key == 0x28 || key == 0x29 || key == 0x2b || key == 0x58 ||
                   (key >= 0x3a && key <= 0x45))
                      ? 0
                      : key;
      if (report.modifiers & 0x55)
        repeating = 0;
      next_repeat = now + 450000;
    }
  }
  bool still_down = false;
  for (int i = 0; i < report.count; ++i)
    if (report.keys[i] == repeating)
      still_down = true;
  if (!still_down)
    repeating = 0;
  if (!fresh && repeating && now >= next_repeat) {
    shell.physical_key(repeating, report.modifiers, report.locks);
    next_repeat = now + 45000;
  }
  held_count = report.count;
  std::memcpy(held, report.keys, sizeof(held));
}
void HidInput::mouse(Shell &shell, const uint8_t *records, int count,
                     uint64_t now) {
  if (!records || count <= 0)
    return;
  if (count > 64)
    count = 64;
  if (count > 1) {
    // Reports may arrive newest first. Preserve short clicks whose press and
    // release both occurred between rendered frames.
    int order[64], used = 0;
    for (int i = 0; i < count; ++i)
      if (records[i * 40 + 8])
        order[used++] = i;
    for (int i = 1; i < used; ++i) {
      int index = order[i], j = i;
      while (j > 0 && value<uint64_t>(records + order[j - 1] * 40) >
                          value<uint64_t>(records + index * 40)) {
        order[j] = order[j - 1];
        --j;
      }
      order[j] = index;
    }
    if (!used) {
      mouse(shell, records, 1, now);
      return;
    }
    for (int i = 0; i < used;) {
      const auto* first = records + order[i] * 40;
      int end = i + 1;
      const auto mask = value<uint32_t>(first + 12) & 7;
      // Stationary edges and wheel routing remain in chronological order.
      // Painting needs intermediate positions; preserve every stroke sample.
      if (!mask && mask == buttons && shell.window != 5 && !value<int32_t>(first + 24)) {
        while (end < used) {
          const auto* next = records + order[end] * 40;
          if ((value<uint32_t>(next + 12) & 7) != mask || value<int32_t>(next + 24)) break;
          ++end;
        }
      }
      if (end == i + 1) mouse(shell, first, 1, now);
      else {
        uint8_t combined[40]; std::memcpy(combined, first, sizeof(combined));
        int64_t dx = 0, dy = 0;
        for (int j = i; j < end; ++j) {
          dx += value<int32_t>(records + order[j] * 40 + 16);
          dy += value<int32_t>(records + order[j] * 40 + 20);
        }
        const int32_t x = int32_t(dx < -4096 ? -4096 : dx > 4096 ? 4096 : dx);
        const int32_t y = int32_t(dy < -4096 ? -4096 : dy > 4096 ? 4096 : dy);
        std::memcpy(combined + 16, &x, sizeof(x)); std::memcpy(combined + 20, &y, sizeof(y));
        mouse(shell, combined, 1, now);
      }
      i = end;
    }
    return;
  }
  // sceMouseRead drains queued reports. A timestamp is not an event ID:
  // several changes (including a stationary click) can share one. Consume
  // every returned record; button edges come from the prior button mask.
  int64_t dx = 0, dy = 0, wheel = 0;
  uint32_t current = buttons;
  bool connected = false, changed = false;
  for (int i = 0; i < count; ++i) {
    auto *r = records + i * 40;
    if (!r[8])
      continue;
    connected = true;
    dx += value<int32_t>(r + 16);
    dy += value<int32_t>(r + 20);
    wheel += value<int32_t>(r + 24);
    changed = true;
    current = value<uint32_t>(r + 12) & 7;
  }
  shell.mouse_connected = connected;
  if (!connected)
    current = 0;
  if (!changed && current == buttons)
    return;
  if (dx == 0 && dy == 0 && wheel == 0 && current == buttons)
    return;
  shell.input_device = InputDevice::mouse;
  shell.focus = -1;
  float scale = .5f + shell.mouse_speed * .1f;
  // Bound malformed device motion before converting to screen coordinates.
  dx = dx < -4096 ? -4096 : dx > 4096 ? 4096 : dx;
  dy = dy < -4096 ? -4096 : dy > 4096 ? 4096 : dy;
  mouse_fraction_x += float(dx) * scale;
  mouse_fraction_y += float(dy) * scale;
  int sx = int(mouse_fraction_x), sy = int(mouse_fraction_y);
  mouse_fraction_x -= sx;
  mouse_fraction_y -= sy;
  uint32_t edge = current & ~buttons;
  bool double_click = false;
  if (edge & 1) {
    double_click = click_time && now - click_time < 400000 &&
                   std::abs(shell.mx + sx - click_x) < 7 &&
                   std::abs(shell.my + sy - click_y) < 7;
    click_time = double_click ? 0 : now;
    click_x = shell.mx + sx;
    click_y = shell.my + sy;
  }
  shell.pointer(shell.mx + sx, shell.my + sy, (edge & 1) != 0,
                (current & 1) != 0, double_click);
  if (wheel && shell.steam_list_bounds.has(shell.mx, shell.my)) {
    // Several wheel reports can arrive together; preserve their distance
    // instead of treating an entire batch as a single notch.
    const int steps = wheel < -64 ? -64 : wheel > 64 ? 64 : int(wheel);
    shell.scroll_steam_library(-3 * steps);
  } else if (wheel && shell.steam_home_bounds.has(shell.mx, shell.my)) {
    const int steps = wheel < -64 ? -64 : wheel > 64 ? 64 : int(wheel);
    shell.scroll_steam_home(-steps);
  } else if (wheel && shell.steam_queue_bounds.has(shell.mx, shell.my)) {
    const int steps = wheel < -64 ? -64 : wheel > 64 ? 64 : int(wheel);
    shell.scroll_steam_queue(-steps);
  }
  if (edge & 2) {
    shell.open_context();
  }
  buttons = current;
}
} // namespace ww
