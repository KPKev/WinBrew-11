#include "../src/shell.hpp"
#include "../src/desktop_context_layout.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    assert(!shell.context && shell.desktop_position_count >= 4);
    assert(shell.desktop_icon_position(11, {0, 0, 108, 94}).y <
           shell.desktop_icon_position(35, {0, 0, 108, 94}).y);
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
    shell.pointer(disabled.x + 80, disabled.y + 50, true);
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
    assert(shell.desktop_icon_position(11, {0, 0, 108, 94}).y <
           shell.desktop_icon_position(35, {0, 0, 108, 94}).y);
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
  {
    const auto testroot = root / "dynamic";
    std::filesystem::create_directories(testroot / "Desktop");
    std::ofstream(testroot / "Desktop" / "example.txt") << "example";
    const auto config = (testroot / "settings.ini").string();
    ww::Shell shell;
    assert(shell.init("assets", nullptr, nullptr, nullptr, config.c_str()));
    shell.desktop_files_refresh();
    auto icon = hit(shell, 2000);
    shell.input_device = ww::InputDevice::mouse;
    shell.pointer(icon.x+5,icon.y+5,false); shell.open_context();
    assert(shell.context && shell.desktop_context_target && !shell.file_context);
    assert(has_hit(shell,1400) && has_hit(shell,1404) && has_hit(shell,1405));
    assert(has_hit(shell,801) && has_hit(shell,802) && has_hit(shell,809));
    click(shell,801);
    assert(shell.file_clipboard.ready && std::filesystem::exists(testroot/"Desktop"/"example.txt"));
    // Stick motion clears D-pad focus; Circle must target the icon under the
    // pointer just as a mouse right-click does.
    shell.draw(); shell.mx=icon.x+1; shell.my=icon.y+5; shell.focus=2000;
    shell.controller_input(true,0,255,128,128,128,0.05f);
    assert(shell.focus==-1 && shell.mx>icon.x && shell.mx<icon.x+icon.w);
    shell.draw(); shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.context && shell.desktop_context_target && has_hit(shell,801) && has_hit(shell,802) && has_hit(shell,809));
    shell.key(ww::Key::back); shell.controller_input(true,0,128,128,128,128,0.016f);
    shell.draw(); shell.focus=2000; shell.input_device=ww::InputDevice::controller;
    shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.context && shell.desktop_context_target && has_hit(shell,809));
    assert(has_hit(shell,1409));
    shell.focus=809; shell.key(ww::Key::accept);
    assert(!std::filesystem::exists(testroot/"Desktop"/"example.txt"));
  }
  {
    ww::Shell shell;
    assert(shell.init("assets",nullptr,nullptr,nullptr,settings.c_str()));
    shell.start=true; shell.all=false; shell.draw();
    for(int i=0;i<shell.count;++i) assert(!(shell.hits[i].id==30 && shell.hits[i].r.y>=510 && shell.hits[i].r.y<935));
    shell.all=true; shell.draw();
    bool found=false;for(int i=0;i<shell.count;++i) if(shell.hits[i].id==30 && shell.hits[i].r.y>=510 && shell.hits[i].r.y<935) found=true;
    assert(!found);
    shell.all=false; shell.start=true; shell.draw();
    ww::Rect vapor{};for(int i=0;i<shell.count;++i) if(shell.hits[i].id==35 && shell.hits[i].r.y>=510 && shell.hits[i].r.y<935) vapor=shell.hits[i].r;
    shell.input_device=ww::InputDevice::controller;shell.focus=35;shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.start&&shell.file_context&&shell.file_menu_kind==6&&shell.shortcut_context_id==35&&shell.focus==813);
    shell.key(ww::Key::back);assert(shell.start&&!shell.file_context);shell.controller_input(true,0,128,128,128,128,0.016f);shell.draw();
    assert(vapor.w);shell.input_device=ww::InputDevice::mouse;shell.pointer(vapor.x+8,vapor.y+8,false);shell.open_context();
    assert(shell.start && !shell.all && shell.shortcut_context_id==35 && shell.file_context && shell.file_menu_kind==6);
    assert(shell.file_context_bounds.x>=vapor.x+vapor.w-8 && has_hit(shell,813));
    const auto pinned_menu=shell.file_context_bounds;shell.pointer(pinned_menu.x+20,pinned_menu.y+20,false);assert(shell.start&&shell.file_context);
    click(shell,813);assert(shell.start && !shell.file_context);
    assert(shell.taskbar_hidden & (1ULL<<55));
    ww::Shell restored;assert(restored.init("assets",nullptr,nullptr,nullptr,settings.c_str()));
    assert(restored.taskbar_hidden & (1ULL<<55));
    restored.start=true;restored.draw();for(int i=0;i<restored.count;++i) assert(!(restored.hits[i].id==35 && restored.hits[i].r.y>=510 && restored.hits[i].r.y<935));
    // All apps keeps built-ins present and exposes independent placement toggles.
    restored.all=true;restored.start=true;restored.draw();
    ww::Rect settings_item{};for(int i=0;i<restored.count;++i)if(restored.hits[i].id==33&&restored.hits[i].r.y>=510&&restored.hits[i].r.y<935)settings_item=restored.hits[i].r;
    assert(settings_item.w && (restored.desktop_hidden&(1ULL<<33)));
    restored.input_device=ww::InputDevice::mouse;restored.pointer(settings_item.x+5,settings_item.y+5,false);restored.open_context();
    assert(restored.start&&restored.all&&restored.shortcut_context_id==33&&restored.file_context&&restored.file_menu_kind==7&&has_hit(restored,820)&&has_hit(restored,821)&&has_hit(restored,822)&&!has_hit(restored,823));
    assert(restored.file_context_bounds.x>=settings_item.x+settings_item.w-8);
    auto placement=hit(restored,820);restored.pointer(placement.x+150,placement.y+15,true);
    assert(restored.start&&restored.all&&!restored.file_context&&!(restored.desktop_hidden&(1ULL<<33)));
    ww::Shell persisted;assert(persisted.init("assets",nullptr,nullptr,nullptr,settings.c_str()));assert(!(persisted.desktop_hidden&(1ULL<<33)));
    persisted.start=true;persisted.all=true;persisted.draw();ww::Rect vapor_all{};
    for(int i=0;i<persisted.count;++i)if(persisted.hits[i].id==35&&persisted.hits[i].r.y>=510&&persisted.hits[i].r.y<935)vapor_all=persisted.hits[i].r;
    assert(vapor_all.w);persisted.input_device=ww::InputDevice::mouse;persisted.pointer(vapor_all.x+5,vapor_all.y+5,false);persisted.open_context();
    assert(persisted.start&&persisted.all&&persisted.shortcut_context_id==35&&persisted.file_menu_kind==7&&!has_hit(persisted,823));
    const auto old_taskbar=persisted.taskbar_hidden & (1ULL<<35);
    click(persisted,821);assert(!(persisted.taskbar_hidden&(1ULL<<55)) && (persisted.taskbar_hidden&(1ULL<<35))==old_taskbar);
    persisted.start=true;persisted.all=false;persisted.draw();bool vapor_pinned=false;
    for(int i=0;i<persisted.count;++i)if(persisted.hits[i].id==35&&persisted.hits[i].r.y>=510&&persisted.hits[i].r.y<935)vapor_pinned=true;
    assert(vapor_pinned);
    persisted.start=true;persisted.all=true;persisted.draw();
    for(int i=0;i<persisted.count;++i)if(persisted.hits[i].id==35&&persisted.hits[i].r.y>=510&&persisted.hits[i].r.y<935)vapor_all=persisted.hits[i].r;
    persisted.pointer(vapor_all.x+5,vapor_all.y+5,false);persisted.open_context();click(persisted,822);
    assert((persisted.taskbar_hidden&(1ULL<<35))!=old_taskbar);
    persisted.start=true;persisted.all=true;persisted.input_device=ww::InputDevice::controller;persisted.draw();persisted.focus=35;persisted.open_context();
    assert(persisted.start&&persisted.file_context&&persisted.focus==820);persisted.draw();persisted.key(ww::Key::down);assert(persisted.focus==821);
    persisted.key(ww::Key::back);assert(persisted.start&&!persisted.file_context);
    persisted.input_device=ww::InputDevice::mouse;persisted.draw();persisted.pointer(vapor_all.x+5,vapor_all.y+5,false);persisted.open_context();
    auto context_area=persisted.file_context_bounds;persisted.pointer(context_area.x+100,context_area.y+20,false);assert(persisted.start&&persisted.file_context);
    persisted.pointer(1600,400,false);assert(!persisted.start&&!persisted.file_context);
  }
  {
    const auto pinned_settings=(root/"pinned-controller.ini").string();
    ww::Shell shell;assert(shell.init("assets",nullptr,nullptr,nullptr,pinned_settings.c_str()));
    shell.start=true;shell.all=false;shell.input_device=ww::InputDevice::controller;shell.draw();shell.focus=33;
    shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.start&&shell.file_context&&shell.file_menu_kind==6&&shell.shortcut_context_id==33&&shell.focus==813&&has_hit(shell,813));
    shell.key(ww::Key::back);assert(shell.start&&!shell.file_context);
    shell.controller_input(true,0,128,128,128,128,0.016f);
    shell.window=1;shell.file_games=true;shell.draw();shell.focus=35;
    shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.start&&shell.file_context&&shell.file_menu_kind==6&&shell.shortcut_context_id==35);
    shell.key(ww::Key::back);shell.controller_input(true,0,128,128,128,128,0.016f);
    shell.window=2;shell.settings_page=8;shell.draw();shell.focus=33;
    shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.start&&shell.file_context&&shell.file_menu_kind==6&&shell.shortcut_context_id==33);
    shell.key(ww::Key::back);shell.controller_input(true,0,128,128,128,128,0.016f);shell.window=0;
    assert(shell.vapor_shortcuts.toggle(987654,ww::VaporShortcuts::Start));
    shell.draw();ww::Rect game{};for(int i=0;i<shell.count;++i)if(shell.hits[i].id==1200&&shell.hits[i].r.y>=510&&shell.hits[i].r.y<935)game=shell.hits[i].r;
    assert(game.w);shell.focus=1200;shell.controller_input(true,0x2000,128,128,128,128,0.016f);
    assert(shell.start&&shell.file_context&&shell.file_menu_kind==6&&shell.shortcut_context_app==987654&&shell.focus==813);
    shell.draw();shell.key(ww::Key::accept);assert(shell.start&&!shell.file_context&&!(shell.vapor_shortcuts.placements(987654)&ww::VaporShortcuts::Start));
  }
  {
    ww::Shell shell;assert(shell.init("assets",nullptr,nullptr,nullptr,(root/"search.ini").string().c_str()));
    shell.start=true;shell.begin_text(ww::TextTarget::search);shell.insert_text("quake");shell.commit_text();
    assert(shell.pending==ww::Action::none&&shell.start);
    shell.activate(30);assert(shell.pending==ww::Action::none);
    shell.search[0]=0;shell.begin_text(ww::TextTarget::search);shell.insert_text("notepad");shell.commit_text();
    assert(shell.window==4&&shell.document.active);
  }
  std::filesystem::remove_all(root);
  std::puts("PASS: desktop context menus, input and preferences");
}
