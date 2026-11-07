// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  const char *file = "build/test-desktop-settings.ini";
  std::remove(file);
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    s.activate(101);
    assert(s.window == 2 && s.settings_page == 1);
    s.activate(111);
    s.activate(114);
    s.activate(117);
    s.activate(119);
    s.activate(121);
    s.activate(123);
    s.activate(124);
    s.activate(115);
    assert(s.dark && s.accent == 2 && s.icon_size == 1 && s.left_taskbar);
    assert(s.background == 1 && !s.show_icons && s.large_pointer && s.clock24);
    assert(!s.dialog);
    s.draw();
  }
  {
    ww::Shell s;
    assert(s.init("assets", nullptr, nullptr, nullptr, file));
    assert(s.dark && s.accent == 2 && s.icon_size == 1 && s.left_taskbar);
    assert(s.background == 1 && !s.show_icons && s.large_pointer && s.clock24);
    s.activate(34);
    s.activate(130);
    s.activate(135);
    s.activate(210);
    s.activate(211);
    assert(!std::strcmp(s.address, "qw"));
    assert(!s.start && s.focus == 211);
    s.draw();
    s.key(ww::Key::right);
    assert(s.focus == 212);
    s.key(ww::Key::down);
    assert(s.focus == 222);
    s.key(ww::Key::erase);
    assert(!std::strcmp(s.address, "q"));
    s.key(ww::Key::back);
    assert(s.window == 3 && !s.address_edit);
    std::snprintf(s.address, sizeof(s.address), "example.com");
    assert(s.navigate());
    assert(s.pending == ww::Action::browser &&
           !std::strcmp(s.url, "https://example.com"));
    std::snprintf(s.address, sizeof(s.address), "quake ii & ps5");
    assert(s.navigate());
    assert(!std::strcmp(
        s.url, "https://www.google.com/search?q=quake%20ii%20%26%20ps5"));
    const char *bad[] = {"file:///etc/passwd", "javascript:alert(1)",
                         "https://",           "https:///bad",
                         "https://name@host/", "https://a b/",
                         "http://a\\b/",       ""};
    for (auto url : bad) {
      s.pending = ww::Action::none;
      std::snprintf(s.address, sizeof(s.address), "%s", url);
      assert(!s.navigate());
      assert(s.pending == ww::Action::none);
    }
    s.activate(130);
    s.activate(135);
    for (int i = 0; i < 300; ++i)
      s.character('a');
    assert(std::strlen(s.address) == 255);
    s.draw();
    assert(s.count < 128);
    s.activate(135);
    s.activate(131);
    assert(s.dialog);
    s.draw();
    int before = s.window;
    s.pointer(20, 25, true, true, true);
    assert(s.window == before);
    s.key(ww::Key::back);
    assert(!s.dialog);
    s.activate(48);
    assert(s.window == 0);
    s.activate(48);
    assert(s.window == 3);
  }
  std::remove(file);
  std::puts(
      "PASS: settings survive restart, controller keyboard navigation, URL "
      "normalization/rejection, input bounds, modal focus and show desktop");
}
