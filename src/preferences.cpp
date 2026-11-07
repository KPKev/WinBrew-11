// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace ww {
Rect Shell::desktop_icon_position(std::uint64_t key, Rect fallback) const {
  for (int i = 0; i < desktop_position_count; ++i)
    if (desktop_positions[i].key == key) { fallback.x = desktop_positions[i].x; fallback.y = desktop_positions[i].y; break; }
  return fallback;
}
void Shell::set_desktop_icon_position(std::uint64_t key, int x, int y) {
  if (!key) return;
  if (x < 0) x = 0; if (x > W - 108) x = W - 108;
  if (y < 0) y = 0; if (y > H - 170) y = H - 170;
  int i = 0; while (i < desktop_position_count && desktop_positions[i].key != key) ++i;
  if (i == desktop_position_count) { if (i >= 133) return; ++desktop_position_count; }
  desktop_positions[i] = {key, x, y};
}
void Shell::load_desktop_icon_positions() {
  if (!preferences[0]) return;
  char path[1100]; if (std::snprintf(path, sizeof(path), "%s.icons", preferences) >= int(sizeof(path))) return;
  FILE* f = std::fopen(path, "rb"); if (!f) return;
  char line[128]; DesktopIconPosition loaded[133]{}; int count = 0;
  bool ok = std::fgets(line, sizeof(line), f) && std::strcmp(line, "V1\n") == 0;
  while (ok && std::fgets(line, sizeof(line), f)) {
    unsigned long long key; int x, y, used = 0;
    if (count == 133 || std::sscanf(line, "%llu %d %d%n", &key, &x, &y, &used) != 3 ||
        line[used] != '\n' || line[used + 1] || x < 0 || x > W - 108 || y < 0 || y > H - 170 ||
        !((key == 10 || key == 11 || key == 13 || key == 34 || key == 35) ||
          ((key >> 32) == 1 && std::uint32_t(key) != 0))) { ok = false; break; }
    for (int i = 0; i < count; ++i) if (loaded[i].key == key) ok = false;
    if (ok) loaded[count++] = {key, x, y};
  }
  if (std::ferror(f)) ok = false; std::fclose(f);
  if (ok) { std::memcpy(desktop_positions, loaded, sizeof(loaded)); desktop_position_count = count; }
}
bool Shell::save_desktop_icon_positions() {
  if (!preferences[0]) return false;
  char path[1100], temp[1120];
  if (std::snprintf(path, sizeof(path), "%s.icons", preferences) >= int(sizeof(path)) ||
      std::snprintf(temp, sizeof(temp), "%s.tmp", path) >= int(sizeof(temp))) return false;
  FILE* f = std::fopen(temp, "wb"); if (!f) return false;
  bool ok = std::fputs("V1\n", f) >= 0;
  for (int i = 0; ok && i < desktop_position_count; ++i)
    ok = std::fprintf(f, "%llu %d %d\n", static_cast<unsigned long long>(desktop_positions[i].key), desktop_positions[i].x, desktop_positions[i].y) > 0;
  if (ok) ok = std::fflush(f) == 0;
#ifdef _WIN32
  if (ok) ok = _commit(_fileno(f)) == 0;
#else
  if (ok) ok = ::fsync(fileno(f)) == 0;
#endif
  if (std::fclose(f) != 0) ok = false;
  if (!ok) return false;
#ifdef _WIN32
  return MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return std::rename(temp, path) == 0;
#endif
}
void Shell::load_preferences() {
  if (!preferences[0])
    return;
  FILE *f = std::fopen(preferences, "rb");
  if (!f)
    return;
  char data[256]{};
  size_t n = std::fread(data, 1, sizeof(data) - 1, f);
  bool readable = std::ferror(f) == 0;
  std::fclose(f);
  if (!readable || n == sizeof(data) - 1)
    return;
  int v = 0, d = 0, icons = 1, left = 0, clock = 0, pointer = 0, size = 0,
      color = 0, bg = 0, speed = 5, cs = 0, safe = 0, volume = 50, mute = 0,
      mouse = 5, deadzone = 22, right = 0, invert = 0, swap = 0,
      touch_deadzone = 4, touch_speed = 5, touch_acceleration = 5, touch_hold = 600;
  int fields = std::sscanf(
      data, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &v, &d,
      &icons, &left, &clock, &pointer, &size, &color, &bg, &speed, &cs, &safe,
      &volume, &mute, &mouse, &deadzone, &right, &invert, &swap,
      &touch_deadzone, &touch_speed, &touch_acceleration, &touch_hold);
  if (!((v == 1 && fields == 9) || (v == 2 && fields == 14) ||
        (v == 3 && fields == 15) || (v == 4 && fields == 19) || (v == 5 && fields == 23)))
    return;
  dark = d == 1;
  show_icons = icons != 0;
  left_taskbar = left == 1;
  clock24 = clock == 1;
  icon_size = size >= 0 && size <= 2 ? size : 0;
  accent = color >= 0 && color <= 2 ? color : 0;
  background = bg >= 0 && bg <= 2 ? bg : 0;
  cursor_size = v == 1 ? (pointer == 1 ? 2 : 0) : (cs >= 0 && cs <= 2 ? cs : 0);
  large_pointer = cursor_size == 2;
  pointer_speed = speed >= 1 && speed <= 10 ? speed : 5;
  safe_area = safe == 16 || safe == 32 ? safe : 0;
  ui_volume = volume >= 0 && volume <= 100 ? volume : 50;
  ui_muted = mute == 1;
  mouse_speed = mouse >= 1 && mouse <= 10 ? mouse : 5;
  pad_deadzone = deadzone >= 10 && deadzone <= 40 ? deadzone : 22;
  pad_right_stick = right == 1;
  pad_invert_y = invert == 1;
  pad_swap_confirm = swap == 1;
  touchpad_deadzone = touch_deadzone >= 0 && touch_deadzone <= 12 ? touch_deadzone : 4;
  touchpad_speed = touch_speed >= 1 && touch_speed <= 10 ? touch_speed : 5;
  touchpad_acceleration = touch_acceleration >= 0 && touch_acceleration <= 10 ? touch_acceleration : 5;
  touchpad_hold_ms = touch_hold >= 400 && touch_hold <= 1400 ? touch_hold : 600;
  save_state = 1;
  pointer_fraction_x = pointer_fraction_y = 0;
  this->pointer(mx, my, false);
}
bool Shell::save_preferences() {
  if (!preferences[0])
    return true;
  char data[256], temp[1050];
  int n =
      std::snprintf(data, sizeof(data),
                    "5 %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                    dark, show_icons, left_taskbar, clock24, large_pointer,
                    icon_size, accent, background, pointer_speed, cursor_size,
                    safe_area, ui_volume, ui_muted, mouse_speed, pad_deadzone,
                    pad_right_stick, pad_invert_y, pad_swap_confirm,
                    touchpad_deadzone, touchpad_speed, touchpad_acceleration, touchpad_hold_ms);
  std::snprintf(temp, sizeof(temp), "%s.tmp", preferences);
  auto fail = [&](const char *stage, int code) {
    save_state = -1;
    save_error = code ? code : EIO;
    std::snprintf(save_stage, sizeof(save_stage), "%s", stage);
    dialog = true;
    std::snprintf(message, sizeof(message),
                  "Not saved (%s, %d). This change lasts for this session.",
                  stage, save_error);
    return false;
  };
  errno = 0;
  FILE *f = std::fopen(temp, "wb");
  if (!f)
    return fail("open", errno);
  bool ok = std::fwrite(data, 1, n, f) == size_t(n) && std::fflush(f) == 0;
  int error = errno;
#ifdef _WIN32
  if (ok) {
    ok = _commit(_fileno(f)) == 0;
    error = errno;
  }
#else
  if (ok) {
    ok = ::fsync(fileno(f)) == 0;
    error = errno;
  }
#endif
  int closed = std::fclose(f);
  if (closed != 0) {
    ok = false;
    error = errno;
  }
  if (!ok)
    return fail("write", error);
  errno = 0;
#ifdef _WIN32
  ok = MoveFileExA(temp, preferences,
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
  error = ok ? 0 : int(GetLastError());
#else
  ok = std::rename(temp, preferences) == 0;
  error = errno;
#endif
  if (!ok)
    return fail("replace", error);
  f = std::fopen(preferences, "rb");
  if (!f)
    return fail("verify open", errno);
  char actual[256]{};
  size_t got = std::fread(actual, 1, sizeof(actual), f);
  ok = got == size_t(n) && std::memcmp(actual, data, n) == 0 &&
       std::ferror(f) == 0;
  std::fclose(f);
  if (!ok)
    return fail("verify", EIO);
  save_state = 1;
  save_error = 0;
  save_stage[0] = 0;
  return true;
}
} // namespace ww
