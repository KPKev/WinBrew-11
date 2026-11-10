// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/hid_input.hpp"
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
template <class T> void put(uint8_t *p, T v) { std::memcpy(p, &v, sizeof(v)); }
static ww::Rect hit(ww::Shell &s, int id) {
  for (int i = 0; i < s.count; ++i)
    if (s.hits[i].id == id)
      return s.hits[i].r;
  assert(false);
  return {};
}
int main() {
  ww::Shell s;
  assert(s.init("assets"));
  ww::HidInput hid;
  uint8_t keyboard[96]{};
  keyboard[16] = 1;
  put<int32_t>(keyboard + 20, 1);
  put<uint16_t>(keyboard + 32, 4);
  hid.keyboard(s, keyboard, 100);
  assert(s.keyboard_connected && !std::strcmp(s.search, "a"));
  hid.keyboard(s, keyboard, 200);
  assert(!std::strcmp(s.search, "a"));
  hid.keyboard(s, keyboard, 450100);
  assert(!std::strcmp(s.search, "aa"));
  keyboard[16] = 0;
  hid.keyboard(s, keyboard, 500000);
  assert(!s.keyboard_connected);
  keyboard[16] = 1;
  put<uint32_t>(keyboard + 28, 2);
  put<uint16_t>(keyboard + 32, 5);
  auto report = ww::parse_keyboard(keyboard);
  assert(report.modifiers == 2 && report.keys[0] == 5);
  s.end_text();
  s.activate(34);
  s.input_device = ww::InputDevice::controller;
  s.activate(130);
  s.clear_text();
  assert(s.touch_keyboard);
  hid.keyboard(s, keyboard, 600000);
  assert(!std::strcmp(s.address, "B") && s.touch_keyboard);
  s.physical_key(4, 1);
  s.physical_key(6);
  assert(!std::strcmp(s.address, "c"));
  s.insert_text("at");
  s.physical_key(0x50);
  s.character('r');
  assert(!std::strcmp(s.address, "cart"));
  s.physical_key(0x4c);
  assert(!std::strcmp(s.address, "car"));
  s.physical_key(0x2a);
  assert(!std::strcmp(s.address, "ca"));
  s.draw();
  auto cap = hit(s, 210);
  s.input_device = ww::InputDevice::mouse;
  s.pointer(cap.x + 8, cap.y + 8, true, true);
  assert(s.touch_keyboard && !std::strcmp(s.address, "caq"));
  s.physical_key(0x1d);
  assert(!std::strcmp(s.address, "caqz") && s.touch_keyboard);
  s.physical_key(0x29);
  assert(!s.touch_keyboard && s.text_target == ww::TextTarget::none);
  s.input_device = ww::InputDevice::mouse;
  s.activate(130);
  assert(!s.touch_keyboard);
  s.physical_key(0x0f, 1);
  assert(s.text_selection && !s.touch_keyboard);
  s.insert_text("example.com");
  s.physical_key(0x28);
  assert(s.pending == ww::Action::browser &&
         !std::strcmp(s.url, "https://example.com"));
  s.pending = ww::Action::none;
  // Every visible point inside each web card has the same action.
  for (int id = 132; id <= 134; ++id) {
    s.activate(34);
    s.draw();
    auto r = hit(s, id);
    for (auto point : {ww::Rect{r.x + 1, r.y + 1, 0, 0},
                       ww::Rect{r.x + r.w - 2, r.y + r.h - 2, 0, 0},
                       ww::Rect{r.x + r.w / 2, r.y + 30, 0, 0},
                       ww::Rect{r.x + 8, r.y + r.h / 2, 0, 0}}) {
      s.pending = ww::Action::none;
      s.draw();
      s.pointer(point.x, point.y, true);
      assert(s.pending == ww::Action::browser);
    }
  }
  s.pending = ww::Action::none;
  s.activate(32);
  s.activate(401);
  s.draw();
  for (int i=0;i<s.count;++i) assert(s.hits[i].id!=30);
  assert(s.pending == ww::Action::none);
  s.pending = ww::Action::none;
  s.activate(100);
  s.draw();
  auto toggle = hit(s, 115);
  bool was = s.clock24;
  s.pointer(toggle.x + 3, toggle.y + 3, true);
  assert(s.clock24 != was);
  // Out-of-order mouse samples sum motion; newest timestamp supplies buttons.
  uint8_t records[80]{};
  records[8] = records[48] = 1;
  put<uint64_t>(records, 20);
  put<int32_t>(records + 16, 3);
  put<uint32_t>(records + 12, 0);
  put<uint64_t>(records + 40, 10);
  put<int32_t>(records + 56, 2);
  put<uint32_t>(records + 52, 0);
  s.pointer(800, 600, false);
  hid.mouse(s, records, 2, 700000);
  assert(s.mx == 805 && s.mouse_connected);
  hid.mouse(s, records, 2, 700001);
  assert(s.mx == 810);
  // Mouse dragging survives an idle controller frame and releases normally.
  s.activate(34);
  s.draw();
  s.pointer(s.wx + 100, s.wy + 20, false);
  int old = s.wx;
  std::memset(records, 0, sizeof(records));
  records[8] = 1;
  put<uint64_t>(records, 30);
  put<uint32_t>(records + 12, 1);
  hid.mouse(s, records, 1, 800000);
  assert(s.drag && s.drag_device == ww::InputDevice::mouse);
  s.move_stick(128, 128, .016f);
  assert(s.drag);
  put<uint64_t>(records, 40);
  put<int32_t>(records + 16, 25);
  hid.mouse(s, records, 1, 810000);
  assert(s.wx == old + 25);
  put<uint64_t>(records, 50);
  put<int32_t>(records + 16, 0);
  put<uint32_t>(records + 12, 0);
  hid.mouse(s, records, 1, 820000);
  assert(!s.drag);
  // Stationary mouse reports never steal controller/keyboard focus.
  s.input_device = ww::InputDevice::controller;
  s.focus = 132;
  put<uint64_t>(records, 60);
  hid.mouse(s, records, 1, 830000);
  assert(s.focus == 132 && s.input_device == ww::InputDevice::controller);
  s.activate(34);
  s.draw();
  auto site = hit(s, 134);
  s.pointer(site.x + 4, site.y + 4, false);
  std::memset(records, 0, sizeof(records));
  records[8] = records[48] = 1;
  put<uint64_t>(records, 80);
  put<uint64_t>(records + 40, 70);
  put<uint32_t>(records + 52, 1);
  hid.mouse(s, records, 2, 840000);
  assert(s.pending == ww::Action::browser);
  // Queued reports may share a timestamp, including across read calls.
  // The taskbar responds to a stationary press after a motion-only report.
  s.pending = ww::Action::none;
  s.activate(34);
  s.draw();
  auto settings = hit(s, 33);
  s.pointer(settings.x + settings.w / 2, settings.y + settings.h / 2, false);
  std::memset(records, 0, sizeof(records));
  records[8] = 1;
  put<uint64_t>(records, 100);
  hid.mouse(s, records, 1, 900000);
  put<uint32_t>(records + 12, 1);
  hid.mouse(s, records, 1, 910000);
  assert(s.window == 2);
  // Held-state repeats must not trigger another click; release with the same
  // timestamp must still release a drag and let the next press act.
  int page = s.settings_page;
  hid.mouse(s, records, 1, 920000);
  assert(s.settings_page == page);
  put<uint32_t>(records + 12, 0);
  hid.mouse(s, records, 1, 930000);
  s.activate(34);
  s.draw();
  s.pointer(s.wx + 100, s.wy + 20, false);
  put<uint32_t>(records + 12, 1);
  hid.mouse(s, records, 1, 940000);
  assert(s.drag);
  put<uint32_t>(records + 12, 0);
  hid.mouse(s, records, 1, 950000);
  assert(!s.drag);
  // A complete equal-timestamp down/up batch and timestamp-zero clicks work.
  s.activate(34);
  s.draw();
  auto youtube = hit(s, 134);
  s.pointer(youtube.x + 8, youtube.y + 8, false);
  records[48] = 1;
  put<uint64_t>(records + 40, 100);
  put<uint32_t>(records + 12, 1);
  put<uint32_t>(records + 52, 0);
  hid.mouse(s, records, 2, 960000);
  assert(s.pending == ww::Action::browser && hid.buttons == 0);
  s.pending = ww::Action::none;
  put<uint64_t>(records, 0);
  put<uint64_t>(records + 40, 0);
  hid.mouse(s, records, 2, 970000);
  assert(s.pending == ww::Action::browser && hid.buttons == 0);
  // Schema 2 migrates, schema 3 round-trips USB speed without resetting theme.
  const char *file = "build/input-preferences.ini";
  FILE *f = std::fopen(file, "wb");
  assert(f);
  std::fputs("2 1 1 0 1 0 0 2 1 7 1 16 60 1\n", f);
  std::fclose(f);
  std::snprintf(s.preferences, sizeof(s.preferences), "%s", file);
  s.load_preferences();
  assert(s.dark && s.mouse_speed == 5 && s.pointer_speed == 7 && s.accent == 2);
  s.mouse_speed = 9;
  assert(s.save_preferences());
  s.mouse_speed = 1;
  s.dark = false;
  s.load_preferences();
  assert(s.dark && s.mouse_speed == 9);
  std::remove(file);
  // High-rate pure motion is consumed in one batch, including newest-first input.
  {
    ww::HidInput batched;
    s.window = 0; s.mouse_speed = 5; s.mx = 500; s.my = 500;
    s.drag = false; s.steam_scroll_drag = s.steam_home_drag = s.steam_queue_drag = false;
    uint8_t reports[64 * 40]{};
    for (int i = 0; i < 64; ++i) {
      auto* r = reports + i * 40; r[8] = 1;
      put<uint64_t>(r, 1000000 + (63 - i) * 1000);
      put<int32_t>(r + 16, 1);
    }
    batched.mouse(s, reports, 64, 2000000);
    assert(s.mx == 564 && s.my == 500);
  }
  std::puts(
      "PASS: mixed HID input, held-key repeat, caret/selection, controller "
      "touch keyboard, full-card clicks, mouse drag and persisted USB speed");
}
