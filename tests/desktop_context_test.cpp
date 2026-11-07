#include "../src/shell.hpp"
#include "../src/desktop_context_layout.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
ww::Rect hit(ww::Shell &shell, int id) {
  shell.draw();
  for (int index = shell.count - 1; index >= 0; --index)
    if (shell.hits[index].id == id) return shell.hits[index].r;
  assert(false);
  return {};
}
bool has_hit(ww::Shell &shell, int id) {
  shell.draw();
  for (int index = 0; index < shell.count; ++index)
    if (shell.hits[index].id == id) return true;
  return false;
}
void click(ww::Shell &shell, int id) {
  auto area = hit(shell, id);
  shell.pointer(area.x + 15, area.y + 15, true);
}
void open(ww::Shell &shell, int x = 600, int y = 350) {
  shell.pointer(x, y, false);
  shell.open_context();
  assert(shell.context && !shell.context_classic && shell.context_submenu == 0);
}
}
int main() {
  const auto root = std::filesystem::path("build") /
      ("desktop-context-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto settings = (root / "settings.ini").string();
  {
    ww::Shell shell;
    assert(shell.init("assets", nullptr, nullptr, nullptr, settings.c_str()));
    open(shell, 1900, 1000);
    auto menu = shell.context_bounds();
    assert(menu.x + menu.w <= ww::W && menu.y + menu.h <= ww::H - 54);
    assert(menu.w == 250 && menu.h == 330);
    assert(hit(shell, 1400).y == menu.y + ww::desktop_context::modern_offsets[0]);
    assert(hit(shell, 1404).y == menu.y + ww::desktop_context::modern_offsets[4]);
    click(shell, 1400);
    assert(shell.context_submenu == 1);
    auto submenu = shell.context_submenu_bounds();
    assert(submenu.x >= 0 && submenu.x + submenu.w <= menu.x + ww::desktop_context::submenu_overlap);
    assert(submenu.w == 264 && submenu.h == 206);
    assert(!has_hit(shell, 1413) && !has_hit(shell, 1414));
    click(shell, 1410);
    assert(!shell.context && shell.icon_size == 2 && shell.save_state == 1);
    {
      ww::Shell restored;
      assert(restored.init("assets", nullptr, nullptr, nullptr, settings.c_str()));
      assert(restored.icon_size == 2);
    }
    open(shell);
    auto view = hit(shell, 1400);
    shell.pointer(view.x + 15, view.y + 15, false);
    assert(shell.context_submenu == 1);
    click(shell, 1415);
    assert(!shell.context && !shell.show_icons);
    open(shell);
    click(shell, 1401);
    assert(shell.context_submenu == 2 && !has_hit(shell, 1410));
    shell.key(ww::Key::back);
    shell.focus = 1401;
    shell.key(ww::Key::right);
    assert(shell.context_submenu == 2 && shell.focus == 1416);
    click(shell, 1416);
    assert(!shell.context && shell.desktop_position_count >= 5);
    assert(shell.desktop_icon_position(13, {0, 0, 108, 94}).y <
           shell.desktop_icon_position(11, {0, 0, 108, 94}).y);
    open(shell);
    click(shell, 1401);
    shell.key(ww::Key::back);
    assert(shell.context && !shell.context_submenu && shell.focus == 1401);
    click(shell, 1408);
    assert(shell.context && shell.context_classic && !shell.context_submenu);
    assert(shell.context_bounds().w == 238 && shell.context_bounds().h == 380);
    assert(hit(shell, 1404).y == shell.context_bounds().y + ww::desktop_context::classic_offsets[11]);
    click(shell, 1404);
    assert(shell.context_submenu == 3);
    auto disabled = shell.context_submenu_bounds();
    assert(disabled.w == 296 && disabled.h == 304);
    shell.draw();
    shell.pointer(disabled.x + 80, disabled.y + 20, true);
    assert(shell.context && shell.context_submenu == 3 && !shell.dialog);
    shell.key(ww::Key::back);
    shell.key(ww::Key::back);
    assert(!shell.context);
    shell.physical_key(0x20, 0x03);
    assert(shell.icon_size == 1);
    open(shell);
    click(shell, 1405);
    assert(!shell.context && shell.window == 2 && shell.settings_page == 0);
    shell.activate(93);
    open(shell);
    click(shell, 1406);
    assert(!shell.context && shell.window == 2 && shell.settings_page == 1);
  }
  {
    ww::Shell shell;
    assert(shell.init("assets", nullptr, nullptr, nullptr, settings.c_str()));
    assert(shell.icon_size == 1 && !shell.show_icons);
    assert(shell.desktop_icon_position(13, {0, 0, 108, 94}).y <
           shell.desktop_icon_position(11, {0, 0, 108, 94}).y);
    open(shell, 1900, 1000);
    click(shell, 1408);
    assert(shell.context_bounds().y + shell.context_bounds().h <= ww::H - 54);
    const auto new_row = hit(shell, 1404);
    click(shell, 1404);
    const auto submenu = shell.context_submenu_bounds();
    assert(submenu.y >= 0 && submenu.y + submenu.h <= ww::H - 54);
    assert(submenu.y + submenu.h == new_row.y + new_row.h);
    assert(submenu.x >= 0 && submenu.x + submenu.w <= shell.context_bounds().x + ww::desktop_context::submenu_overlap);
  }
  {
    ww::Shell shell;
    assert(shell.init("assets", nullptr, nullptr, nullptr, settings.c_str()));
    for (bool dark : {false, true}) for (int x : {0, ww::W - 1}) for (int y : {0, ww::H - 1}) {
      shell.dark = dark;
      for (bool classic : {false, true}) for (int item : {1400, 1401, 1404}) {
        shell.context = false;
        open(shell, x, y);
        if (classic) shell.activate(1408);
        shell.activate(item);
        const auto root_menu = shell.context_bounds();
        const auto submenu = shell.context_submenu_bounds();
        assert(root_menu.x >= 0 && root_menu.y >= 0 && root_menu.x + root_menu.w <= ww::W &&
               root_menu.y + root_menu.h <= ww::H - 54);
        assert(submenu.x >= 0 && submenu.y >= 0 && submenu.x + submenu.w <= ww::W &&
               submenu.y + submenu.h <= ww::H - 54);
        shell.draw();
      }
    }
  }
  std::filesystem::remove_all(root);
  std::puts("PASS: desktop context menus, input and preferences");
}
