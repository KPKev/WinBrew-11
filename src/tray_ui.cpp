// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cstdio>
namespace ww {
void Shell::tray_ui() {
  if (quick) {
    Rect r{1496, 586, 408, 430};
    panel(r);
    add(r, 89);
    text(r.x + 24, r.y + 24, "Quick settings", 1, ink());
    icon(r.x + 24, r.y + 79, 30, 11);
    text(r.x + 74, r.y + 76, network_name, 1, ink());
    text(r.x + 24, r.y + 123,
         network_state == 3   ? "Local network connected"
         : network_state == 0 ? "Disconnected"
                              : "Network status unavailable",
         0, muted());
    text(r.x + 24, r.y + 155,
         network_ip[0] ? network_ip : "No local IP address", 0, muted());
    button({r.x + 240, r.y + 152, 142, 36}, 105, "Network >");
    text(r.x + 24, r.y + 218, "WinBrew 11 sounds", 1, ink());
    char level[24];
    std::snprintf(level, sizeof(level), "%d%%", ui_volume);
    if (audio_available) {
      button({r.x + 20, r.y + 260, 45, 42}, 170, "-");
      round({r.x + 78, r.y + 278, 224, 5}, 2, muted());
      round({r.x + 78, r.y + 278, 224 * ui_volume / 100, 5}, 2, accent_color());
      round({r.x + 72 + 224 * ui_volume / 100, r.y + 272, 17, 17}, 8,
            accent_color());
      button({r.x + 321, r.y + 260, 45, 42}, 171, "+");
      text(r.x + 315, r.y + 218, ui_muted ? "Muted" : level, 0, muted());
      button({r.x + 20, r.y + 322, 161, 42}, 172, ui_muted ? "Unmute" : "Mute");
      button({r.x + 205, r.y + 322, 177, 42}, 173, "Test sound");
    } else
      text(r.x + 24, r.y + 268, "Desktop audio unavailable", 0, muted());
    text(r.x + 24, r.y + 391, "Game and TV volume: use PS5 sound controls", 0,
         muted());
  }
  if (tray) {
    Rect r{1488, 696, 416, 320};
    panel(r);
    add(r, 89);
    brand_icon(r.x + 24, r.y + 24, 34, true);
    text(r.x + 76, r.y + 30, "WinBrew 11 is running", 1, ink());
    text(r.x + 24, r.y + 87, "Desktop windows", 0, muted());
    int local = window ? window : restored;
    if (local) {
      icon(r.x + 24, r.y + 134, 30, local == 1 ? 5 : local == 2 ? 6 : 8);
      button({r.x + 68, r.y + 127, 315, 44},
             local == 1   ? 32
             : local == 2 ? 33
                          : 34,
             local == 1   ? "File Explorer"
             : local == 2 ? "Settings"
                          : "Web");
      text(r.x + 76, r.y + 181, window ? "Open" : "Minimized", 0, muted());
    } else
      text(r.x + 24, r.y + 142, "No desktop windows open", 0, muted());
    text(r.x + 24, r.y + 230, "Active PS5 application", 0, muted());
    text(r.x + 24, r.y + 269,
         active_title[0] ? active_title : "Status unavailable", 0, ink());
  }
}
} // namespace ww
