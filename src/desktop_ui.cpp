// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include "desktop_context_layout.hpp"
#include "owner_check_dispatch.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <map>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace ww {
static uint32_t color(int r, int g, int b) {
  return 0xff000000u | uint32_t(b) << 16 | uint32_t(g) << 8 | r;
}
static void taskbar_start_icon(Shell &shell, int x, int y) {
  constexpr int pane = 14;
  constexpr int gap = 1;
  constexpr int colors[4][2][3] = {
      {{96, 207, 255}, {63, 170, 226}},
      {{51, 174, 243}, {24, 139, 219}},
      {{49, 157, 230}, {0, 120, 215}},
      {{10, 132, 221}, {0, 119, 211}}};
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const int column = quadrant % 2;
    const int row = quadrant / 2;
    for (int py = 0; py < pane; ++py)
      for (int px = 0; px < pane; ++px) {
        if ((column == 0 ? px == 0 : px == pane - 1) &&
            (row == 0 ? py == 0 : py == pane - 1))
          continue;
        const int blend = px + py;
        constexpr int extent = 2 * (pane - 1);
        const int red = (colors[quadrant][0][0] * (extent - blend) + colors[quadrant][1][0] * blend) / extent;
        const int green = (colors[quadrant][0][1] * (extent - blend) + colors[quadrant][1][1] * blend) / extent;
        const int blue = (colors[quadrant][0][2] * (extent - blend) + colors[quadrant][1][2] * blend) / extent;
        shell.rect({x + column * (pane + gap) + px, y + row * (pane + gap) + py, 1, 1}, color(red, green, blue));
      }
  }
}
static int menu_text_width(const Shell &shell, const char *value, int scale = 7) {
  int advance = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
    const unsigned character = *p >= 32 && *p < 127 ? *p : '?';
    advance += shell.font[(character - 32) * 48 * 56];
  }
  return (advance * scale + 4) / 9;
}
static void menu_text(Shell &shell, int x, int y, const char *value,
                      uint32_t foreground, int scale = 7) {
  int advance = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
    const unsigned character = *p >= 32 && *p < 127 ? *p : '?';
    const auto *glyph = shell.font + (character - 32) * 48 * 56;
    const int start = x + (advance * scale + 4) / 9;
    const int glyph_width = ((glyph[0] + 3) * scale + 8) / 9;
    const int glyph_height = (22 * scale + 8) / 9;
    for (int dy = 0; dy < glyph_height; ++dy) {
      const int source_y = dy * 9 / scale + 1, fy = dy * 9 % scale;
      for (int dx = 0; dx < glyph_width; ++dx) {
        const int source_x = dx * 9 / scale, fx = dx * 9 % scale;
        if (source_x >= 47 || source_y >= 55) continue;
        const int alpha = (glyph[source_y * 48 + source_x] * (scale - fx) * (scale - fy) +
                           glyph[source_y * 48 + source_x + 1] * fx * (scale - fy) +
                           glyph[(source_y + 1) * 48 + source_x] * (scale - fx) * fy +
                           glyph[(source_y + 1) * 48 + source_x + 1] * fx * fy) / (scale * scale);
        if (alpha) shell.rect({start + dx, y + dy, 1, 1}, (uint32_t(alpha) << 24) | (foreground & 0xffffff));
      }
    }
    advance += glyph[0];
  }
}
uint32_t Shell::surface() const {
  return dark ? color(32, 34, 40) : color(243, 246, 251);
}
uint32_t Shell::ink() const {
  return dark ? color(244, 245, 249) : color(28, 31, 37);
}
uint32_t Shell::muted() const {
  return dark ? color(173, 181, 196) : color(95, 103, 117);
}
uint32_t Shell::accent_color() const {
  return accent == 1   ? color(128, 76, 203)
         : accent == 2 ? color(0, 130, 116)
                       : color(0, 103, 192);
}
void Shell::text_fit(int x, int y, int available, const char *value, int size,
                     uint32_t c) {
  char label[768];
  std::snprintf(label, sizeof(label), "%s", value);
  if (width(label, size) > available) {
    auto n = std::strlen(label);
    while (n && width(label, size) + width("...", size) > available) {
      --n;
      while (n && (static_cast<unsigned char>(label[n]) & 0xc0) == 0x80)
        --n;
      label[n] = 0;
    }
    std::snprintf(label + n, sizeof(label) - n, "...");
  }
  text(x, y, label, size, c);
}
// Only http(s) URLs reach the system service. Text is encoded as a search
// query.
bool Shell::navigate() {
  const char *p = address;
  while (*p == ' ')
    ++p;
  if (!*p)
    return false;
  for (const char *s = p; *s; ++s)
    if ((unsigned char)*s < 32 || *s == 127)
      return false;
  bool explicit_scheme = std::strstr(p, "://") != nullptr;
  bool http = !std::strncmp(p, "https://", 8) || !std::strncmp(p, "http://", 7);
  if (explicit_scheme && !http)
    return false;
  if (http) {
    const char *host = p + (p[4] == 's' ? 8 : 7);
    if (!*host || *host == '/' || *host == ' ' || std::strchr(p, ' ') ||
        std::strchr(p, '\\') || std::strchr(p, '@'))
      return false;
    std::snprintf(url, sizeof(url), "%s", p);
  } else if (std::strchr(p, ':'))
    return false;
  else if (std::strchr(p, '.') && !std::strchr(p, ' '))
    std::snprintf(url, sizeof(url), "https://%s", p);
  else {
    std::snprintf(url, sizeof(url), "https://www.google.com/search?q=");
    size_t n = std::strlen(url);
    for (; *p && n + 3 < sizeof(url); ++p) {
      unsigned char c = *p;
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
        url[n++] = c;
      else {
        std::snprintf(url + n, 4, "%%%02X", c);
        n += 3;
      }
    }
    url[n] = 0;
  }
  end_text();
  pending = Action::browser;
  return true;
}
void Shell::settings_ui(Rect wr) {
  int x = wr.x, y = wr.y;
  const char *pages[] = {
      "System",          "Personalization", "Taskbar",
      "Mouse & pointer", "About",           "Network",
      "Input & typing",  "PS5 controller",  "Controller test", "Touchpad"};
  text(x + 28, y + 74, "WinBrew 11", 2, ink());
  text(x + 28, y + 116, "Make it yours", 0, muted());
  for (int i = 0; i < 8; ++i) {
    Rect r{x + 16, y + 177 + i * 53, 244, 44};
    if (settings_page == i || (i == 7 && (settings_page == 8 || settings_page == 9))) {
      round(r, 5, dark ? color(53, 57, 67) : color(224, 231, 241));
      round({r.x, r.y + 12, 3, 20}, 1, accent_color());
    }
    add(r, 100 + i);
    if (hover == 100 + i || focus == 100 + i)
      round(r, 5, dark ? color(60, 65, 76) : color(230, 236, 245));
    const int kinds[] = {1, 0, 5, 1, 6, 11, 6, 16};
    icon(r.x + 15, r.y + 12, 20, kinds[i]);
    text(r.x + 49, r.y + 14, pages[i], 0, ink());
  }
  text(x + 28, y + wr.h - 72, "WinBrew 11 desktop", 0, muted());
  int cx = x + 300, cw = wr.w - 332;
  text(cx, y + 76, pages[settings_page], 2, ink());
  if (save_state)
    text(cx + cw - 300, y + 89,
         save_state == 1 ? "Preferences saved" : "Preferences not saved", 0,
         save_state == 1 ? muted() : color(181, 62, 36));
  auto card = [&](int top, int h) {
    round({cx, y + top, cw, h}, 7,
          dark ? color(48, 51, 59) : color(255, 255, 255));
  };
  auto choice = [&](Rect r, int id, const char *label, bool selected) {
    add(r, id);
    round(r, 5,
          selected ? accent_color()
          : dark   ? color(63, 68, 80)
                   : color(232, 237, 245));
    center(r.x, r.y + 13, r.w, label, 0, selected ? 0xffffffff : ink());
    if (focus == id)
      round({r.x + 5, r.y + r.h + 3, r.w - 10, 2}, 1, accent_color());
  };
  auto toggle = [&](int top, int id, const char *title, const char *sub,
                    bool on) {
    add({cx, y + top, cw, 88}, id);
    card(top, 88);
    if (hover == id || focus == id)
      round({cx, y + top, cw, 88}, 7,
            dark ? color(60, 65, 76) : color(230, 236, 245));
    text(cx + 22, y + top + 17, title, 1, ink());
    text(cx + 22, y + top + 49, sub, 0, muted());
    Rect r{cx + cw - 92, y + top + 27, 58, 30};
    round(r, 15, on ? accent_color() : muted());
    round({r.x + (on ? 31 : 3), r.y + 3, 24, 24}, 12, 0xffffffff);
    if (focus == id)
      round({r.x - 3, r.y + 35, 64, 2}, 1, accent_color());
  };
  if (settings_page == 0) {
    card(142, 118);
    icon(cx + 24, y + 167, 62, 1);
    text(cx + 110, y + 163, "Your PS5 desktop", 1, ink());
    text(cx + 110, y + 200, "WinBrew 11  |  Desktop edition", 0, muted());
    toggle(278, 115, "24-hour clock", "Use 00:00 through 23:59 on the taskbar",
           clock24);
    card(384, 94);
    text(cx + 22, y + 401, "Display", 1, ink());
    text(cx + 22, y + 435, "1920 x 1080  |  Desktop canvas", 0, muted());
    card(496, 106);
    text(cx + 22, y + 514, "Games keep their own video settings", 1, ink());
    text(cx + 22, y + 551, "Desktop preferences apply to WinBrew 11 only.", 0,
         muted());
    button({cx, y + 630, 200, 42}, 101, "Personalize desktop");
  } else if (settings_page == 1) {
    card(142, 110);
    text(cx + 22, y + 159, "Choose your mode", 1, ink());
    text(cx + 22, y + 198, "Windows and menus", 0, muted());
    choice({cx + cw - 260, y + 178, 106, 44}, 110, "Light", !dark);
    choice({cx + cw - 142, y + 178, 106, 44}, 111, "Dark", dark);
    card(268, 112);
    text(cx + 22, y + 286, "Accent color", 1, ink());
    const char *names[] = {"Blue", "Violet", "Teal"};
    for (int i = 0; i < 3; ++i) {
      button({cx + 245 + i * 150, y + 309, 136, 44}, 112 + i, names[i]);
      if (accent == i)
        round({cx + 278 + i * 150, y + 354, 70, 3}, 1, accent_color());
    }
    card(396, 112);
    text(cx + 22, y + 414, "Background", 1, ink());
    const char *bg[] = {"Bloom", "Midnight", "Slate"};
    for (int i = 0; i < 3; ++i) {
      button({cx + 245 + i * 150, y + 437, 136, 44}, 120 + i, bg[i]);
      if (background == i)
        round({cx + 278 + i * 150, y + 482, 70, 3}, 1, accent_color());
    }
    card(524, 112);
    text(cx + 22, y + 542, "Desktop icon size", 1, ink());
    choice({cx + cw - 290, y + 563, 120, 44}, 116, "Small", icon_size == 0);
    choice({cx + cw - 155, y + 563, 120, 44}, 117, "Medium", icon_size == 1);
  } else if (settings_page == 2) {
    card(142, 118);
    text(cx + 22, y + 160, "Taskbar alignment", 1, ink());
    text(cx + 22, y + 198, "Position your pinned apps", 0, muted());
    choice({cx + cw - 280, y + 181, 120, 44}, 118, "Center", !left_taskbar);
    choice({cx + cw - 145, y + 181, 120, 44}, 119, "Left", left_taskbar);
    toggle(278, 123, "Show desktop icons", "Keep shortcuts on your desktop",
           show_icons);
    toggle(384, 115, "24-hour clock", "Choose the time format you prefer",
           clock24);
    card(490, 110);
    text(cx + 22, y + 508, "Pinned apps", 1, ink());
    text(cx + 22, y + 545, "Start, Search, Explorer, Web, Games and Settings",
         0, muted());
  } else if (settings_page == 3) {
    card(142, 166);
    text(cx + 22, y + 160, "Pointer size", 1, ink());
    const char *sizes[] = {"Small", "Medium", "Large"};
    for (int i = 0; i < 3; ++i) {
      cursor(cx + 290 + i * 140, y + 171, i == 0 ? 22 : i == 1 ? 28 : 36);
      choice({cx + 240 + i * 140, y + 237, 124, 42}, 140 + i, sizes[i],
             cursor_size == i);
    }
    text(cx + 22, y + 208, "Full arrow and outline", 0, muted());
    card(326, 116);
    text(cx + 22, y + 343, "Controller pointer", 1, ink());
    text(cx + 22, y + 381, "Speed, stick and button preferences", 0, muted());
    button({cx + cw - 230, y + 365, 208, 44}, 107, "PS5 controller >");
    card(460, 116);
    text(cx + 22, y + 478, "Screen edge margin", 1, ink());
    text(cx + 22, y + 516, "Keep the whole pointer visible", 0, muted());
    for (int i = 0; i < 3; ++i)
      choice({cx + 390 + i * 140, y + 501, 124, 42}, 145 + i,
             i == 0   ? "None"
             : i == 1 ? "16 px"
                      : "32 px",
             safe_area == i * 16);
    card(594, 100);
    text(cx + 22, y + 613,
         pad_swap_confirm ? "D-pad: focus  |  Circle: select  |  Cross: right-click"
                          : "D-pad: focus  |  Cross: select  |  Circle: right-click",
         0, ink());
    text(cx + 22, y + 651, "Options: Start  |  Triangle: game library", 0,
         muted());
  } else if (settings_page == 4) {
    card(142, 462);
    brand_icon(cx + 28, y + 166, 112, true);
    text(cx + 164, y + 181, "WinBrew 11", 3, ink());
    text(cx + 164, y + 238, "Desktop update 16", 0, muted());
    text(cx + 28, y + 310, "Created by KPKev", 1, ink());
    text(cx + 28, y + 349, "Developed with AI assistance.", 0, muted());
    text(cx + 28, y + 401, "A familiar Windows 11 inspired desktop for PS5", 0,
         muted());
    text(cx + 28, y + 445, "Windows games use Playden's Wine-based runtime.", 0,
         muted());
    text(cx + 28, y + 489,
         "Open-source contributors credited in the included licenses.", 0,
         muted());
    text(cx + 28, y + 551,
         "Independent software. Microsoft Windows is not installed.", 0,
         muted());
    card(620, 126);
    text(cx + 22, y + 638, "Game return", 1, ink());
    button({cx + cw - 176, y + 635, 154, 38}, 185, "Run check");
    text(cx + 22, y + 681, owner_check_line(owner_check_status), 0, ink());
    text(cx + 22, y + 715, "Checks only. Your game files and settings stay unchanged.", 0, muted());
  } else if (settings_page == 8) {
    controller_test_ui(wr);
  } else if (settings_page == 9) {
    card(142, 96);
    text(cx + 22, y + 158, "Touchpad", 1, ink());
    text(cx + 22, y + 196, "Hover to focus. Press to open; two-finger tap right-clicks.", 0, muted());
    button({cx + cw - 190, y + 151, 168, 40}, 107, "< Controller");
    auto touch_adjust = [&](int top, const char* label, const char* description, int minus, int plus, const char* value) {
      card(top, 96); text(cx + 22, y + top + 14, label, 1, ink());
      text(cx + 22, y + top + 52, description, 0, muted());
      button({cx + 520, y + top + 26, 48, 42}, minus, "-");
      center(cx + 580, y + top + 36, 120, value, 0, ink());
      button({cx + 715, y + top + 26, 48, 42}, plus, "+");
    };
    char dz[24], speed[24], accel[24], hold[24];
    std::snprintf(dz, sizeof(dz), "%d / 12", touchpad_deadzone);
    std::snprintf(speed, sizeof(speed), "%d / 10", touchpad_speed);
    std::snprintf(accel, sizeof(accel), "%d / 10", touchpad_acceleration);
    std::snprintf(hold, sizeof(hold), "%.1f sec", double(touchpad_hold_ms) / 1000.0);
    touch_adjust(255, "Dead zone", "Filters small unintentional finger motion", 320, 321, dz);
    touch_adjust(364, "Pointer speed", "Sets how far the pointer moves", 322, 323, speed);
    touch_adjust(473, "Acceleration", "Faster finger movement travels farther", 324, 325, accel);
    touch_adjust(582, "Long press", "Hold to pick up or drop", 326, 327, hold);
    button({cx + 22, y + 696, 190, 42}, 328, "Reset touchpad");
  } else if (settings_page == 7) {
    card(142, 104);
    icon(cx + 22, y + 170, 48, 16);
    text(cx + 86, y + 163, "DualSense", 1, ink());
    text(cx + 86, y + 204,
         pad_connected ? "Connected to PS5" : "Waiting for controller input", 0,
         muted());
    button({cx + cw - 180, y + 153, 158, 34}, 309, "Test input");
    button({cx + cw - 180, y + 197, 158, 34}, 308, "Reset defaults");
    button({cx + cw - 350, y + 153, 158, 34}, 329, "Touchpad");
    auto adjust = [&](int top, const char *title, const char *detail, int minus,
                      int plus, int value, int maximum, const char *label) {
      card(top, 100);
      text(cx + 22, y + top + 17, title, 1, ink());
      text(cx + 22, y + top + 55, detail, 0, muted());
      button({cx + 350, y + top + 37, 48, 42}, minus, "-");
      round({cx + 418, y + top + 52, 230, 10}, 5,
            dark ? color(85, 93, 107) : color(215, 222, 234));
      round({cx + 418, y + top + 52, 230 * value / maximum, 10}, 5,
            accent_color());
      button({cx + 668, y + top + 37, 48, 42}, plus, "+");
      text(cx + 740, y + top + 51, label, 0, ink());
    };
    char speed[20], deadzone[20];
    std::snprintf(speed, sizeof(speed), "%d / 10", pointer_speed);
    std::snprintf(deadzone, sizeof(deadzone), "%d%%", pad_deadzone * 100 / 128);
    adjust(264, "Pointer speed", "Desktop pointer movement", 300, 301,
           pointer_speed, 10, speed);
    adjust(376, "Stick deadzone", "Reduce unwanted pointer drift", 302, 303,
           pad_deadzone, 40, deadzone);
    card(488, 92);
    text(cx + 22, y + 505, "Pointer stick", 1, ink());
    text(cx + 22, y + 541, "Choose the stick that moves the pointer", 0,
         muted());
    choice({cx + cw - 274, y + 514, 118, 42}, 304, "Left stick",
           !pad_right_stick);
    choice({cx + cw - 144, y + 514, 122, 42}, 305, "Right stick",
           pad_right_stick);
    auto compact_toggle = [&](int top, int id, const char *title,
                              const char *detail, bool on) {
      Rect row{cx, y + top, cw, 64};
      add(row, id);
      card(top, 64);
      if (hover == id || focus == id)
        round(row, 7, dark ? color(60, 65, 76) : color(230, 236, 245));
      text(cx + 22, y + top + 11, title, 0, ink());
      text(cx + 22, y + top + 36, detail, 0, muted());
      Rect sw{cx + cw - 92, y + top + 17, 58, 30};
      round(sw, 15, on ? accent_color() : muted());
      round({sw.x + (on ? 31 : 3), sw.y + 3, 24, 24}, 12, 0xffffffff);
    };
    compact_toggle(596, 306, "Invert vertical pointer movement",
                   "Desktop only; games keep their own settings", pad_invert_y);
    compact_toggle(672, 307, "Swap primary and secondary",
                   pad_swap_confirm ? "Circle selects  |  Cross right-clicks"
                                    : "Cross selects  |  Circle right-clicks",
                   pad_swap_confirm);
  } else if (settings_page == 6) {
    card(142, 132);
    text(cx + 22, y + 161, "Connected devices", 1, ink());
    text(cx + 22, y + 207,
         keyboard_connected ? "USB keyboard: connected"
                            : "USB keyboard: not detected",
         0, muted());
    text(cx + 440, y + 207,
         mouse_connected ? "USB mouse: connected" : "USB mouse: not detected",
         0, muted());
    card(292, 116);
    text(cx + 22, y + 309, "Mouse speed", 1, ink());
    text(cx + 22, y + 347, "USB mouse or receiver", 0, muted());
    button({cx + 260, y + 333, 48, 44}, 180, "-");
    for (int i = 0; i < 10; ++i)
      round({cx + 326 + i * 35, y + 349, 28, 12}, 4,
            i < mouse_speed ? accent_color() : muted());
    button({cx + 692, y + 333, 48, 44}, 181, "+");
    char speed[12];
    std::snprintf(speed, sizeof(speed), "%d / 10", mouse_speed);
    text(cx + 753, y + 346, speed, 0, ink());
    card(426, 228);
    text(cx + 22, y + 445, "Type your way", 1, ink());
    text(cx + 22, y + 490,
         "Select a text field with the controller for a touch keyboard.", 0,
         muted());
    text(cx + 22, y + 530,
         "Mouse, keyboard and controller stay available together.", 0, muted());
    text(cx + 22, y + 570,
         "Ctrl+L: web address  |  Ctrl+A: select all  |  Enter: open", 0,
         muted());
    text(cx + 22, y + 610, "These controls apply inside the WinBrew 11 desktop.",
         0, muted());
  } else {
    card(142, 160);
    icon(cx + 28, y + 169, 48, 11);
    text(cx + 102, y + 168, network_name, 1, ink());
    text(cx + 102, y + 209,
         network_state == 3   ? "Connected to local network"
         : network_state == 0 ? "Disconnected"
         : network_state > 0  ? "Connecting"
                              : "Status unavailable",
         0, muted());
    text(cx + 28, y + 262,
         network_ip[0] ? network_ip : "No IPv4 address available", 0, muted());
    card(320, 116);
    text(cx + 22, y + 340, "Internet access", 1, ink());
    text(cx + 22, y + 379, "Open a web page to check access to the internet.",
         0, muted());
    button({cx + 22, y + 464, 196, 44}, 34, "Open Web");
    button({cx + 238, y + 464, 196, 44}, 183, "Test browsing");
    text(cx + 22, y + 551,
         "Wi-Fi and system network options are managed by PS5.", 0, muted());
  }
}
void Shell::browser_ui(Rect wr) {
  int x = wr.x, y = wr.y, cx = x + wr.w / 2;
  round({x + 14, y + 52, wr.w - 150, 44}, 22,
        dark ? color(52, 56, 65) : color(233, 237, 244));
  icon(x + 29, y + 62, 24, 7);
  draw_text_field(x + 68, y + 64, wr.w - 220, TextTarget::address,
                  "Search Google or type a web address");
  add({x + 14, y + 52, wr.w - 150, 44}, 130);
  button({x + wr.w - 122, y + 52, 108, 44}, 131, "Open >");
  {
    center(x, y + 181, wr.w, "Browse the web", 3, ink());
    center(x, y + 245, wr.w, "Search with Google or enter a website above", 0,
           muted());
    const char *names[] = {"Google", "Wikipedia", "YouTube"};
    for (int i = 0; i < 3; ++i) {
      Rect r{cx - 315 + i * 220, y + 326, 190, 132};
      add(r, 132 + i);
      round(r, 9, dark ? color(48, 51, 59) : color(237, 241, 247));
      if (hover == 132 + i || focus == 132 + i)
        round(r, 9, dark ? color(66, 73, 87) : color(215, 229, 247));
      icon(r.x + 75, r.y + 19, 40, i == 0 ? 8 : i == 1 ? 9 : 10);
      center(r.x, r.y + 92, r.w, names[i], 0, ink());
    }
    text(x + 235, y + 483, "Recent websites opened from WinBrew 11", 1, ink());
    if (web_history.count) button({x + wr.w - 170, y + 480, 130, 34}, 855, "Clear");
    if (!web_history.count)
      text(x + 235, y + 526, "No recent websites yet.", 0, muted());
    for (int i = 0; i < 5 && web_history_page * 5 + i < web_history.count; ++i) {
      Rect r{x + 235, y + 520 + i * 34, wr.w - 480, 32};
      add(r, 850 + i);
      if (hover == 850 + i || focus == 850 + i)
        round(r, 5, dark ? color(66, 73, 87) : color(215, 229, 247));
      text_fit(r.x + 8, r.y + 4, r.w - 16, web_history.sites[web_history_page * 5 + i], 0, ink());
    }
    if (web_history_page > 0) button({x + wr.w - 220, y + 526, 74, 34}, 856, "< Prev");
    if ((web_history_page + 1) * 5 < web_history.count)
      button({x + wr.w - 138, y + 526, 74, 34}, 857, "Next >");
    if (web_history.dirty) {
      text(x + 235, y + 700, "Recent sites not saved.", 0, ink());
      button({x + 500, y + 694, 90, 34}, 858, "Retry");
    } else if (web_history.error)
      text(x + 235, y + 700, "Recent sites are unavailable on this device.", 0, muted());
    else
      text(x + 235, y + 700, "Paths, searches, browser navigation and cookies are not stored.", 0, muted());
  }
}
static void desktop_context_ui(Shell &shell) {
  using namespace desktop_context;
  const bool classic = shell.context_classic;
  const Rect menu = shell.context_bounds();
  const auto primary = shell.dark ? color(241, 243, 245) : color(28, 31, 32);
  const auto disabled = shell.dark ? color(139, 143, 147) : color(143, 150, 152);
  const auto secondary = shell.dark ? color(177, 181, 185) : color(95, 103, 105);
  const auto separator = shell.dark ? color(72, 76, 80) : color(199, 207, 207);
  const auto blue = shell.accent_color();
  auto panel = [&](Rect area) {
    if (classic) {
      shell.round({area.x + 2, area.y + 3, area.w, area.h}, 4, 0x28000000);
      shell.round(area, 4, shell.dark ? color(77, 80, 84) : color(198, 198, 198));
      shell.round({area.x + 1, area.y + 1, area.w - 2, area.h - 2}, 3,
                  shell.dark ? color(44, 46, 49) : color(250, 250, 250));
      return;
    }
    int red = 0, green = 0, blue_sample = 0, count = 0;
    for (int row = 0; row < 6; ++row) for (int column = 0; column < 6; ++column) {
      const int x = area.x + (2 * column + 1) * area.w / 12;
      const int y = area.y + (2 * row + 1) * area.h / 12;
      if (x < 0 || x >= W || y < 0 || y >= H) continue;
      const auto sample = shell.pixels[y * W + x];
      red += sample & 255; green += (sample >> 8) & 255; blue_sample += (sample >> 16) & 255; ++count;
    }
    if (!count) count = 1;
    red /= count; green /= count; blue_sample /= count;
    const int luminance = (red * 30 + green * 59 + blue_sample * 11) / 100;
    const auto fill = shell.dark ? color(43, 46, 49) :
        color((247 * 84 + luminance * 16 + (red - luminance) * 4) / 100,
              (254 * 84 + luminance * 16 + (green - luminance) * 4) / 100,
              (246 * 84 + luminance * 16 + (blue_sample - luminance) * 4) / 100);
    for (int spread = 5; spread > 0; --spread)
      shell.round({area.x - spread, area.y - spread + 3, area.w + 2 * spread, area.h + 2 * spread},
                  8 + spread, 0x06000000);
    shell.round(area, 8, shell.dark ? color(70, 73, 77) : color(212, 219, 219));
    shell.round({area.x + 1, area.y + 1, area.w - 2, area.h - 2}, 7, fill);
  };
  auto box = [&](int x, int y, int width, int height, uint32_t ink) {
    shell.rect({x, y, width, 1}, ink); shell.rect({x, y + height - 1, width, 1}, ink);
    shell.rect({x, y, 1, height}, ink); shell.rect({x + width - 1, y, 1, height}, ink);
  };
  auto row = [&](Rect area, int id, const char *label, bool enabled, int label_offset,
                 const char *shortcut, bool arrow, bool expanded = false) {
    if (enabled) shell.add(area, id);
    if (enabled && (shell.hover == id || shell.focus == id || expanded))
      shell.round(area, classic ? 0 : 4,
                  shell.dark ? color(60, 64, 68) : (classic ? color(229, 229, 229) : color(205, 218, 217)));
    const int text_y = area.y + (classic ? 3 : 6);
    menu_text(shell, area.x + label_offset, text_y, label, enabled ? primary : disabled);
    if (shortcut) menu_text(shell, area.x + area.w - menu_text_width(shell, shortcut, 6) - 8,
                            text_y + 1, shortcut, secondary, 6);
    if (arrow) {
      const int x = area.x + area.w - 13, y = area.y + area.h / 2;
      shell.line(x - 2, y - 4, x + 1, y, secondary);
      shell.line(x + 1, y, x - 2, y + 4, secondary);
    }
  };
  auto menu_icon = [&](int kind, int x, int y, bool enabled) {
    const auto pen = enabled ? (shell.dark ? color(226, 230, 230) : color(64, 72, 73)) : disabled;
    const auto accent = enabled ? blue : disabled;
    if (kind == 0) {
      for (int column = 0; column < 2; ++column)
        for (int row_index = 0; row_index < 2; ++row_index)
          box(x + column * 8, y + row_index * 8, 5, 5, pen);
    } else if (kind == 1) {
      shell.line(x + 4, y + 1, x + 4, y + 14, pen);
      shell.line(x + 1, y + 4, x + 4, y + 1, pen); shell.line(x + 4, y + 1, x + 7, y + 4, pen);
      shell.line(x + 11, y + 1, x + 11, y + 14, accent);
      shell.line(x + 8, y + 11, x + 11, y + 14, accent); shell.line(x + 11, y + 14, x + 14, y + 11, accent);
    } else if (kind == 2) {
      shell.line(x + 4, y + 2, x + 9, y + 1, pen); shell.line(x + 9, y + 1, x + 13, y + 4, pen);
      shell.line(x + 13, y + 4, x + 14, y + 9, pen); shell.line(x + 14, y + 9, x + 11, y + 13, pen);
      shell.line(x + 11, y + 13, x + 6, y + 14, pen); shell.line(x + 6, y + 14, x + 2, y + 11, pen);
      shell.line(x + 2, y + 11, x + 1, y + 6, pen); shell.line(x + 1, y + 6, x + 4, y + 2, pen);
      shell.line(x + 9, y + 1, x + 9, y + 4, pen); shell.line(x + 9, y + 1, x + 13, y + 1, pen);
    } else if (kind == 3) {
      shell.line(x + 1, y + 5, x + 8, y + 5, accent); shell.line(x + 1, y + 5, x + 5, y + 1, accent);
      shell.line(x + 1, y + 5, x + 5, y + 9, accent); shell.line(x + 8, y + 5, x + 12, y + 6, accent);
      shell.line(x + 12, y + 6, x + 13, y + 10, accent); shell.line(x + 13, y + 10, x + 9, y + 14, accent);
    } else if (kind == 4) {
      shell.line(x + 4, y + 1, x + 11, y + 1, pen); shell.line(x + 11, y + 1, x + 14, y + 4, pen);
      shell.line(x + 14, y + 4, x + 14, y + 11, pen); shell.line(x + 14, y + 11, x + 11, y + 14, pen);
      shell.line(x + 11, y + 14, x + 4, y + 14, pen); shell.line(x + 4, y + 14, x + 1, y + 11, pen);
      shell.line(x + 1, y + 11, x + 1, y + 4, pen); shell.line(x + 1, y + 4, x + 4, y + 1, pen);
      shell.line(x + 7, y + 4, x + 7, y + 11, accent); shell.line(x + 4, y + 7, x + 11, y + 7, accent);
    } else if (kind == 5) {
      box(x, y + 2, 14, 11, pen); shell.line(x + 5, y + 13, x + 5, y + 15, pen);
      shell.line(x + 3, y + 15, x + 10, y + 15, pen); shell.round({x + 10, y + 9, 6, 6}, 3, accent);
    } else if (kind == 6) {
      shell.line(x + 3, y + 13, x + 12, y + 3, pen); shell.line(x + 4, y + 14, x + 13, y + 4, pen);
      shell.line(x + 1, y + 15, x + 4, y + 13, accent, 2);
    } else if (kind == 7) {
      shell.rect({x, y + 2, 15, 12}, pen);
      const auto white = color(248, 248, 248);
      shell.line(x + 3, y + 5, x + 5, y + 7, white); shell.line(x + 5, y + 7, x + 3, y + 9, white);
      shell.line(x + 7, y + 10, x + 11, y + 10, white);
    } else {
      box(x + 1, y + 4, 11, 11, pen); shell.line(x + 7, y + 8, x + 14, y + 1, accent);
      shell.line(x + 10, y + 1, x + 14, y + 1, accent); shell.line(x + 14, y + 1, x + 14, y + 5, accent);
    }
  };
  auto file_icon = [&](int index, int x, int y) {
    if (index == 0) {
      shell.rect({x + 1, y + 4, 13, 10}, color(235, 183, 60));
      shell.rect({x + 1, y + 2, 6, 4}, color(246, 202, 90));
      shell.rect({x + 2, y + 6, 12, 7}, color(250, 205, 90));
      return;
    }
    const uint32_t shades[] = {color(0, 120, 215), color(185, 51, 45), color(117, 80, 157),
                               color(67, 134, 189), color(43, 91, 169), color(185, 51, 45),
                               color(195, 74, 38), color(7, 129, 119), color(130, 137, 140),
                               color(27, 117, 64), color(221, 174, 61)};
    box(x + 2, y + 1, 11, 14, shell.dark ? color(176, 181, 184) : color(142, 150, 154));
    shell.rect({x + 3, y + 2, 9, 12}, color(250, 250, 250));
    shell.rect({x + 1, y + 7, 8, 7}, shades[index - 1]);
    const char letters[] = {'>', 'A', 'H', 'I', 'W', 'A', 'P', 'P', '-', 'X', 'Z'};
    char label[2] = {letters[index - 1], 0};
    menu_text(shell, x + 2, y + 5, label, color(255, 255, 255), 5);
  };
  panel(menu);
  shell.add(menu, 89);
  const char *modern[] = {"View", "Sort by", "Refresh", "Undo Rename", "New", "Display settings",
                          "Personalize", "Open in Terminal", "Show more options"};
  const int modern_ids[] = {1400, 1401, 1402, -1, 1404, 1405, 1406, -1, 1408};
  const char *classic_names[] = {"View", "Sort by", "Refresh", "Paste", "Undo Rename", "Open in Terminal",
      "Open with Visual Studio", "Open with Cursor", "PDF24", "NVIDIA App", "NVIDIA Control Panel",
      "New", "Display settings", "Personalize"};
  const int classic_ids[] = {1400, 1401, 1402, -1, -1, -1, -1, -1, -1, -1, -1, 1404, 1405, 1406};
  const int *offsets = classic ? classic_offsets : modern_offsets;
  const int rows = classic ? 14 : 9, height = classic ? classic_row_height : modern_row_height;
  for (int index = 0; index < rows; ++index) {
    const int id = classic ? classic_ids[index] : modern_ids[index];
    const bool arrow = id == 1400 || id == 1401 || id == 1404;
    const bool expanded = shell.context_submenu &&
        ((id == 1400 && shell.context_submenu == 1) || (id == 1401 && shell.context_submenu == 2) ||
         (id == 1404 && shell.context_submenu == 3));
    const char *shortcut = ((!classic && index == 3) || (classic && index == 4)) ? "Ctrl+Z" : nullptr;
    const Rect area{menu.x + 4, menu.y + offsets[index], menu.w - 8, height};
    row(area, id, classic ? classic_names[index] : modern[index], id >= 0,
        classic ? 32 : 39, shortcut, arrow, expanded);
    if (!classic) menu_icon(index, menu.x + 16, area.y + 7, id >= 0);
    else if (index == 5 || index == 12 || index == 13)
      menu_icon(index == 5 ? 7 : index == 12 ? 5 : 6, menu.x + 10, area.y + 4, id >= 0);
    else if (index >= 6 && index <= 10) {
      const int icon_x = menu.x + 9, icon_y = area.y + 4;
      if (index == 6) {
        const auto purple = color(128, 82, 190);
        shell.line(icon_x + 2, icon_y + 4, icon_x + 12, icon_y + 1, purple, 2);
        shell.line(icon_x + 2, icon_y + 4, icon_x + 12, icon_y + 13, purple, 2);
        shell.line(icon_x + 2, icon_y + 11, icon_x + 12, icon_y + 1, purple, 2);
        shell.line(icon_x + 2, icon_y + 11, icon_x + 12, icon_y + 13, purple, 2);
        shell.line(icon_x + 12, icon_y + 1, icon_x + 12, icon_y + 13, purple, 2);
      } else if (index == 7) {
        shell.round({icon_x, icon_y, 15, 15}, 3, color(31, 34, 38));
        shell.line(icon_x + 3, icon_y + 4, icon_x + 11, icon_y + 7, color(235, 237, 239));
        shell.line(icon_x + 11, icon_y + 7, icon_x + 6, icon_y + 12, color(235, 237, 239));
      } else if (index == 8) {
        shell.round({icon_x + 3, icon_y + 3, 10, 10}, 5, color(178, 142, 56));
        shell.line(icon_x + 2, icon_y + 9, icon_x + 13, icon_y + 9, color(55, 53, 46));
      } else {
        shell.rect({icon_x, icon_y, 15, 15}, color(32, 35, 32));
        shell.line(icon_x + 2, icon_y + 6, icon_x + 7, icon_y + 3, color(113, 182, 45));
        shell.line(icon_x + 7, icon_y + 3, icon_x + 13, icon_y + 6, color(113, 182, 45));
        shell.line(icon_x + 13, icon_y + 6, icon_x + 7, icon_y + 11, color(113, 182, 45));
        shell.line(icon_x + 7, icon_y + 11, icon_x + 2, icon_y + 6, color(113, 182, 45));
        shell.round({icon_x + 6, icon_y + 6, 3, 3}, 1, color(113, 182, 45));
      }
    }
  }
  const int modern_separators[] = {104, 176, 248, 288};
  const int classic_separators[] = {80, 232, 264, 296, 328};
  const int *separators = classic ? classic_separators : modern_separators;
  for (int index = 0; index < (classic ? 5 : 4); ++index)
    shell.rect({menu.x + 5, menu.y + separators[index], menu.w - 10, 1}, separator);
  if (!shell.context_submenu) return;
  const Rect sub = shell.context_submenu_bounds();
  panel(sub);
  shell.add(sub, 89);
  if (shell.context_submenu == 1) {
    const char *names[] = {"Large icons", "Medium icons", "Small icons", "Auto arrange icons",
                           "Align icons to grid", "Show desktop icons"};
    const char *keys[] = {"Ctrl+Shift+2", "Ctrl+Shift+3", "Ctrl+Shift+4", nullptr, nullptr, nullptr};
    const int *positions = classic ? classic_view_offsets : modern_view_offsets;
    for (int index = 0; index < 6; ++index) {
      const bool enabled = index < 3 || index == 5;
      const bool checked = index < 3 ? shell.icon_size == 2 - index : index == 5 && shell.show_icons;
      const Rect area{sub.x + 4, sub.y + positions[index], sub.w - 8, height};
      row(area, 1410 + index, names[index], enabled, 66, keys[index], false);
      const int icon_x = sub.x + 44, icon_y = area.y + (height - 15) / 2;
      if (index < 3) {
        box(icon_x + 1, icon_y + 1, index == 2 ? 10 : 12, index == 2 ? 10 : 9, enabled ? primary : disabled);
        if (index != 2) shell.line(icon_x + 4, icon_y + 12, icon_x + 10, icon_y + 12, secondary);
        else { box(icon_x + 1, icon_y + 1, 4, 4, primary); box(icon_x + 7, icon_y + 7, 4, 4, primary); }
      } else if (index == 3 || index == 4) menu_icon(index == 3 ? 1 : 0, icon_x, icon_y, enabled);
      else {
        shell.line(icon_x, icon_y + 7, icon_x + 4, icon_y + 3, primary);
        shell.line(icon_x + 4, icon_y + 3, icon_x + 10, icon_y + 3, primary);
        shell.line(icon_x + 10, icon_y + 3, icon_x + 14, icon_y + 7, primary);
        shell.line(icon_x + 14, icon_y + 7, icon_x + 10, icon_y + 11, primary);
        shell.line(icon_x + 10, icon_y + 11, icon_x + 4, icon_y + 11, primary);
        shell.line(icon_x + 4, icon_y + 11, icon_x, icon_y + 7, primary);
        shell.round({icon_x + 6, icon_y + 5, 4, 4}, 2, blue);
      }
      if (checked && index < 3) shell.round({sub.x + 16, area.y + height / 2 - 2, 4, 4}, 2, secondary);
      else if (checked) {
        shell.line(sub.x + 15, area.y + height / 2, sub.x + 18, area.y + height / 2 + 3, secondary);
        shell.line(sub.x + 18, area.y + height / 2 + 3, sub.x + 23, area.y + height / 2 - 3, secondary);
      }
    }
    shell.rect({sub.x + 5, sub.y + positions[3] - 3, sub.w - 10, 1}, separator);
    shell.rect({sub.x + 5, sub.y + positions[5] - 3, sub.w - 10, 1}, separator);
  } else if (shell.context_submenu == 2) {
    const char *names[] = {"Name", "Size", "Item type", "Date modified"};
    for (int index = 0; index < 4; ++index) {
      const Rect area{sub.x + 4, sub.y + 4 + index * (classic ? 24 : 32), sub.w - 8, height};
      row(area, 1416, names[index], index == 0, 11, nullptr, false);
    }
  } else {
    const char *names[] = {"Folder", "Shortcut", "Microsoft Access Database", "AutoHotkey Script", "Bitmap image",
        "Microsoft Word Document", "Microsoft Access Database", "Microsoft PowerPoint Presentation",
        "Microsoft Publisher Document", "Text Document", "Microsoft Excel Worksheet", "Compressed (zipped) Folder"};
    for (int index = 0; index < 12; ++index) {
      const int top = classic ? 4 + index * 24 + (index >= 2 ? 4 : 0) : 4 + index * 32;
      const Rect area{sub.x + 4, sub.y + top, sub.w - 8, height};
      row(area, -1, names[index], false, classic ? 33 : 38, nullptr, false);
      file_icon(index, sub.x + 13, area.y + (height - 16) / 2);
    }
    if (classic) shell.rect({sub.x + 4, sub.y + 54, sub.w - 8, 1}, separator);
  }
}
void Shell::desktop_ui() {
  if (background == 0)
    std::memcpy(pixels, wallpaper, W * H * 4);
  else
    rect({0, 0, W, H}, background == 1 ? color(12, 23, 44) : color(49, 62, 78));
  count = 0;
  hover = -1;
  const char *hint = nullptr;
  Rect hint_target{};
  auto describe = [&](Rect r, const char *label) {
    if (r.has(mx, my)) {
      hint = label;
      hint_target = r;
    }
  };
  for (auto &app : rendered_shortcut_apps) app = 0;
  auto& shortcut_labels = desktop_shortcut_labels;
  const auto desktop_state = steam ? steam->status() : SteamStatus{};
  std::uint64_t label_context = 14695981039346656037ull;
  auto label_hash = [&](std::uint64_t value) { label_context = (label_context ^ value) * 1099511628211ull; };
  label_hash(desktop_state.game_count); label_hash(unsigned(desktop_state.account)); label_hash(vapor_shortcuts.count());
  for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
    const auto app = vapor_shortcuts.entries()[i].app;
    rendered_shortcut_apps[i] = app; label_hash(app);
  }
  const auto label_now = std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
  if (shortcut_labels_source != steam || shortcut_labels_context != label_context || label_now >= shortcut_labels_next_scan_ms) {
    shortcut_labels_source = steam; shortcut_labels_context = label_context;
    shortcut_labels_next_scan_ms = label_now + (steam ? steam->library_poll_interval_ms() : 250);
    std::map<std::uint32_t, std::size_t> wanted;
    for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
      const auto app = vapor_shortcuts.entries()[i].app;
      shortcut_labels[i] = "Steam game " + std::to_string(app); wanted.emplace(app, i);
    }
    if (steam) for (std::size_t n = 0; n < desktop_state.game_count && !wanted.empty(); ++n) {
      SteamGame game;
      if (!steam->game_summary(n, game)) continue;
      const auto found = wanted.find(game.app_id);
      if (found != wanted.end()) { shortcut_labels[found->second] = game.title; wanted.erase(found); }
    }
  }
  const char *labels[] = {"This PC", "Recycle Bin", "Vapor",
                          "Quake II RTX", "Web"};
  int kinds[] = {1, 2, 16, 4, 8}, ids[] = {10, 11, 35, 13, 34};
  if (show_icons)
    for (int i = 0; i < 5; ++i) {
      int spacing = icon_size == 2 ? 134 : icon_size == 1 ? 112 : 94;
      int s = icon_size == 2 ? 64 : icon_size == 1 ? 46 : 36;
      Rect r = desktop_icon_position(std::uint64_t(ids[i]), {12, 18 + i * spacing, 108, spacing - 5});
      add(r, ids[i]);
      if (hover == ids[i] || focus == ids[i])
        round(r, 4, 0x406fabed);
      icon(r.x + (r.w - s) / 2, r.y + 4, s, kinds[i]);
      center(r.x + 1, r.y + s + 15, r.w, labels[i], 0, color(10, 30, 55));
      center(r.x, r.y + s + 14, r.w, labels[i], 0, 0xffffffff);
    }
  if (show_icons) {
    int row = 0, spacing = icon_size == 2 ? 134 : icon_size == 1 ? 112 : 94;
    int size = icon_size == 2 ? 64 : icon_size == 1 ? 46 : 36;
    for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
      if (!(vapor_shortcuts.entries()[i].placements & VaporShortcuts::Desktop)) continue;
      Rect r = desktop_icon_position(0x100000000ULL | vapor_shortcuts.entries()[i].app,
                                    {12, 18 + (5 + row++) * spacing, 108, spacing - 5});
      add(r, 1200 + int(i));
      if (hover == 1200 + int(i) || focus == 1200 + int(i)) round(r, 4, 0x406fabed);
      vapor_shortcut_icon(vapor_shortcuts.entries()[i].app, r.x + (r.w - size) / 2, r.y + 4, size, shortcut_labels[i].c_str());
      text_fit(r.x + 2, r.y + size + 14, r.w - 4, shortcut_labels[i].c_str(), 0, 0xffffffff);
    }
  }
  Rect wr = maximized ? Rect{0, 0, W, H - 54} : Rect{wx, wy, 1200, 780};
  if (window) {
    panel(wr);
    add(wr, 89);
    icon(wr.x + 17, wr.y + 11, 22, window == 1 ? (file_games ? 16 : 5) : window == 2 ? 6 : 8);
    text(wr.x + 50, wr.y + 12,
         window == 1   ? (file_games ? "Vapor" : "File Explorer")
         : window == 2 ? "Settings"
         : window == 4 ? "Notepad"
         : window == 5 ? "WinBrew 11 Paint"
                       : "Web - New tab",
         0, ink());
    add({wr.x, wr.y, wr.w - 150, 42}, 90);
    button({wr.x + wr.w - 150, wr.y, 50, 42}, 91, "");
    button({wr.x + wr.w - 100, wr.y, 50, 42}, 92, "");
    button({wr.x + wr.w - 50, wr.y, 50, 42}, 93, "");
    describe({wr.x + wr.w - 150, wr.y, 50, 42}, "Minimize");
    describe({wr.x + wr.w - 100, wr.y, 50, 42},
             maximized ? "Restore down" : "Maximize");
    describe({wr.x + wr.w - 50, wr.y, 50, 42}, "Close");
    if (hover == 93)
      rect({wr.x + wr.w - 50, wr.y, 50, 42}, color(196, 43, 28));
    int controls = wr.x + wr.w;
    line(controls - 131, wr.y + 23, controls - 120, wr.y + 23, ink());
    if (maximized) {
      line(controls - 78, wr.y + 15, controls - 68, wr.y + 15, ink());
      line(controls - 68, wr.y + 15, controls - 68, wr.y + 25, ink());
      line(controls - 70, wr.y + 25, controls - 68, wr.y + 25, ink());
      line(controls - 81, wr.y + 18, controls - 71, wr.y + 18, ink());
      line(controls - 81, wr.y + 18, controls - 81, wr.y + 28, ink());
      line(controls - 71, wr.y + 18, controls - 71, wr.y + 28, ink());
      line(controls - 81, wr.y + 28, controls - 71, wr.y + 28, ink());
    } else {
      line(controls - 81, wr.y + 16, controls - 70, wr.y + 16, ink());
      line(controls - 81, wr.y + 16, controls - 81, wr.y + 27, ink());
      line(controls - 70, wr.y + 16, controls - 70, wr.y + 27, ink());
      line(controls - 81, wr.y + 27, controls - 70, wr.y + 27, ink());
    }
    line(controls - 31, wr.y + 16, controls - 20, wr.y + 27,
         hover == 93 ? 0xffffffff : ink());
    line(controls - 20, wr.y + 16, controls - 31, wr.y + 27,
         hover == 93 ? 0xffffffff : ink());
    if (window == 2)
      settings_ui(wr);
    else if (window == 3)
      browser_ui(wr);
    else if (window == 4)
      notepad_ui(wr);
    else if (window == 5)
      paint.draw(*this, wr);
    else
      explorer_ui(wr);
  }
  if (start) {
    Rect r{660, 365, 600, 645};
    panel(r);
    add(r, 89);
    round({692, 394, 536, 42}, 21,
          dark ? color(49, 53, 62) : color(255, 255, 255));
    icon(709, 405, 22, 7);
    draw_text_field(750, 407, 458, TextTarget::search, "Search for apps");
    add({692, 394, 536, 42}, 43);
    text(704, 469,
         search[0] ? "Search results"
         : all     ? "All apps"
                   : "Pinned",
         1, ink());
    button({1090, 458, 130, 36}, 42, all ? "< Back" : "All apps >");
    const char *names[] = {"Quake II RTX", "Vapor", "File Explorer",
                           "Settings", "Web"};
    const char *match[] = {"quake ii rtx", "vapor game library games",
                           "file explorer", "settings",
                           "web chrome browser google"};
    int k[] = {4, 16, 5, 6, 8}, id[] = {30, 35, 32, 33, 34};
    int row = 0;
    for (int i = 0; i < 5; ++i) {
      if (search[0] && !std::strstr(match[i], search))
        continue;
      if (all || search[0]) {
        Rect b{692, 514 + row++ * 68, 536, 58};
        add(b, id[i]);
        if (hover == id[i] || focus == id[i])
          round(b, 5, dark ? color(55, 60, 70) : color(223, 233, 246));
        icon(b.x + 18, b.y + 11, 34, k[i]);
        text(b.x + 74, b.y + 19, names[i], 0, ink());
      } else {
        Rect b{692 + (i % 3) * 178, 514 + (i / 3) * 115, 168, 102};
        add(b, id[i]);
        if (hover == id[i] || focus == id[i])
          round(b, 5, dark ? color(55, 60, 70) : color(223, 233, 246));
        icon(b.x + 66, b.y + 9, 36, k[i]);
        center(b.x, b.y + 61, b.w, names[i], 0, ink());
      }
    }
    int pinned = 0;
    if (!all && !search[0]) for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
      if (!(vapor_shortcuts.entries()[i].placements & VaporShortcuts::Start)) continue;
      int pos = 5 + pinned++;
      Rect b{692 + (pos % 3) * 178, 514 + (pos / 3) * 115, 168, 102};
      add(b, 1200 + int(i));
      if (hover == 1200 + int(i) || focus == 1200 + int(i)) round(b, 5, dark ? color(55, 60, 70) : color(223, 233, 246));
      vapor_shortcut_icon(vapor_shortcuts.entries()[i].app, b.x + 66, b.y + 9, 36, shortcut_labels[i].c_str());
      text_fit(b.x + 5, b.y + 61, b.w - 10, shortcut_labels[i].c_str(), 0, ink());
    }
    if (!all && !search[0] && pinned <= 1) {
      text(704, 777, "Recommended", 1, ink());
      icon(708, 832, 34, 4);
      text(762, 829, "Quake II RTX", 0, ink());
      text(762, 858, "Continue playing", 0, muted());
      add({692, 820, 536, 70}, 30);
    } else if ((all || search[0]) && row == 0)
      text(712, 548, "No apps found", 0, muted());
    rect({661, 935, 598, 1}, dark ? color(65, 69, 79) : color(216, 224, 235));
    brand_icon(705, 957, 30, true);
    text(751, 963, "KPKev", 0, ink());
    button({1113, 949, 110, 44}, 44, "Close");
  }
  if (context) desktop_context_ui(*this);
  rect({0, H - 54, W, 54}, dark ? color(31, 34, 42) : color(231, 239, 249));
  rect({0, H - 54, W, 1}, dark ? color(61, 67, 81) : color(253, 254, 255));
  int tx = left_taskbar ? 18 : 690;
  auto task = [&](int dx, int kind, int id, bool active, const char *label, std::uint32_t game_app = 0) {
    Rect r{tx + dx, H - 49, 43, 43};
    add(r, id);
    describe(r, label);
    if (hover == id || focus == id || active)
      round(r, 5, dark ? color(57, 62, 74) : color(251, 253, 255));
    if (id == 40) taskbar_start_icon(*this, r.x + 7, r.y + 7);
    else if (game_app) vapor_shortcut_icon(game_app, r.x + 7, r.y + 7, 29, label);
    else icon(r.x + 7, r.y + 7, 29, kind);
    if (active)
      round({r.x + 14, H - 6, 16, 3}, 1, accent_color());
  };
  task(0, 0, 40, start, "Start");
  round({tx + 55, H - 45, 206, 36}, 18,
        dark ? color(48, 53, 64) : color(250, 252, 255));
  add({tx + 55, H - 45, 206, 36}, 41);
  describe({tx + 55, H - 45, 206, 36}, "Search");
  icon(tx + 68, H - 37, 21, 7);
  text(tx + 102, H - 35, "Search", 0, muted());
  task(273, 5, 32, window == 1 && !file_games, "File Explorer");
  task(326, 8, 34, window == 3, "Web");
  task(379, 16, 35, window == 1 && file_games, "Vapor");
  task(432, 4, 30, false, "Quake II RTX");
  task(485, 6, 33, window == 2, "Settings");
  int task_pin = 0;
  for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
    if (vapor_shortcuts.entries()[i].placements & VaporShortcuts::Taskbar)
      task(538 + 53 * task_pin++, vapor_shortcuts.entries()[i].app == 1089130 ? 4 : 16,
           1200 + int(i), false, shortcut_labels[i].c_str(), vapor_shortcuts.entries()[i].app);
  }
  button({W - 339, H - 49, 38, 43}, 151, "^");
  describe({W - 339, H - 49, 38, 43}, "Show hidden icons");
  text(W - 292, H - 34, "ENG", 0, muted());
  add({W - 245, H - 49, 73, 43}, 150);
  describe({W - 245, H - 49, 73, 43}, "Network and sound");
  if (hover == 150 || focus == 150 || quick)
    round({W - 245, H - 49, 73, 43}, 5,
          dark ? color(57, 62, 74) : color(251, 253, 255));
  icon(W - 238, H - 38, 24, 11);
  icon(W - 204, H - 38, 24, 12);
  text(W - 158, H - 45, clock, 0, ink());
  text(W - 158, H - 23, date, 0, ink());
  add({W - 169, H - 53, 160, 53}, 45);
  add({W - 7, H - 53, 7, 53}, 48);
  describe({W - 169, H - 53, 160, 53}, "Date and time");
  describe({W - 7, H - 53, 7, 53}, "Show desktop");
  if (calendar) {
    Rect r{1490, 510, 408, 500};
    panel(r);
    add(r, 89);
    const char *months[] = {"January",   "February", "March",    "April",
                            "May",       "June",     "July",     "August",
                            "September", "October",  "November", "December"};
    char label[48];
    std::snprintf(label, sizeof(label), "%s %d", months[month - 1], year);
    text(1515, 536, "Calendar", 1, ink());
    text(1515, 591, label, 1, ink());
    button({1790, 582, 40, 40}, 46, "<");
    button({1845, 582, 40, 40}, 47, ">");
    const char *week[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    for (int i = 0; i < 7; ++i)
      center(1508 + i * 53, 648, 48, week[i], 0, muted());
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = 1;
    std::mktime(&tm);
    int days = month == 2 ? 28 + (year % 4 == 0 &&
                                  (year % 100 != 0 || year % 400 == 0))
               : (month == 4 || month == 6 || month == 9 || month == 11) ? 30
                                                                         : 31;
    for (int d = 1; d <= days; ++d) {
      int at = d - 1 + tm.tm_wday, x = 1508 + (at % 7) * 53,
          y = 691 + (at / 7) * 47;
      bool today = d == day && month == todayMonth && year == todayYear;
      if (today)
        round({x + 4, y - 4, 40, 40}, 20, accent_color());
      std::snprintf(label, sizeof(label), "%d", d);
      center(x, y + 5, 48, label, 0, today ? 0xffffffff : ink());
    }
  }
  tray_ui();
  keyboard_ui();
  notepad_prompt_ui();
  file_actions_ui();
  if (hint && !dialog && !file_context && !file_properties &&
      !file_transfer.busy() && !start && !context && !quick && !tray && !calendar &&
      !touch_keyboard && !drag) {
    int w = width(hint) + 28, x = hint_target.x + (hint_target.w - w) / 2;
    if (x < 8)
      x = 8;
    if (x + w > W - 8)
      x = W - 8 - w;
    int y = hint_target.y - 50;
    if (y < 8)
      y = hint_target.y + hint_target.h + 8;
    round({x + 1, y + 3, w, 40}, 7, 0x24000000);
    round({x, y, w, 40}, 7, dark ? color(77, 82, 94) : color(205, 211, 220));
    round({x + 1, y + 1, w - 2, 38}, 6,
          dark ? color(43, 46, 54) : color(249, 250, 252));
    center(x, y + 10, w, hint, 0, ink());
  }
  if (dialog) {
    rect({0, 0, W, H}, 0x38000000);
    panel({560, 398, 800, 264});
    if (steam_manage_confirmation_app) {
      text(596, 425, steam_manage_reinstall ? "Reinstall game" : "Remove game", 2, ink());
      text(596, 480, "The game and its PS5 Home entry will be removed.", 0, ink());
      text(596, 512, steam_manage_reinstall ? "Saves are kept; the download will start again." : "Your saves and settings will be kept.", 0, ink());
      button({1025, 589, 130, 42}, 60, "Cancel", 0);
      button({1170, 589, 150, 42}, 61, steam_manage_reinstall ? "Reinstall" : "Remove", 1);
    } else if (steam_setup_confirmation_app) {
      text(596, 425, "Finish game setup", 2, ink());
      text(596, 480, "Close Vapor once so the PS5 can register this game.", 0, ink());
      text(596, 512, "Then reopen Vapor and launch it with Play.", 0, ink());
      button({1025, 589, 130, 42}, 60, "Cancel", 0);
      button({1170, 589, 150, 42}, 61, "Continue", 1);
    } else {
      text(596, 425, "WinBrew 11", 2, ink());
      text(596, 486, message, 0, ink());
      button({1185, 589, 130, 42}, 60, "OK", 1);
    }
  }
  if (pad_touch_drag_locked && !dialog) {
    const char* label = "Dragging - hold touchpad to drop";
    const int w = width(label) + 20;
    int x = mx + 24, y = my + 24;
    if (x + w > W - 8) x = W - w - 8;
    if (y + 32 > H - 54) y = my - 40;
    round({x, y, w, 32}, 4, 0xe8211a17);
    text(x + 10, y + 5, label, 0, 0xffffffff);
  }
  cursor(mx, my, cursor_size == 2 ? 36 : cursor_size == 1 ? 28 : 22);
}
} // namespace ww
