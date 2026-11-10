#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
namespace fs = std::filesystem;
namespace {
void put(const fs::path &p, const std::string &text) {
  FILE *f = std::fopen(p.string().c_str(), "wb");
  assert(f);
  assert(std::fwrite(text.data(), 1, text.size(), f) == text.size());
  assert(std::fclose(f) == 0);
}
std::string get(const fs::path &p) {
  FILE *f = std::fopen(p.string().c_str(), "rb");
  assert(f);
  std::string out;
  char bytes[4096];
  while (auto n = std::fread(bytes, 1, sizeof(bytes), f))
    out.append(bytes, n);
  std::fclose(f);
  return out;
}
ww::Rect hit(ww::Shell &s, int id) {
  s.draw();
  for (int i = s.count - 1; i >= 0; --i)
    if (s.hits[i].id == id)
      return s.hits[i].r;
  assert(false);
  return {};
}
bool has_hit(ww::Shell &s, int id) {
  s.draw();
  for (int i = 0; i < s.count; ++i)
    if (s.hits[i].id == id)
      return true;
  return false;
}
int entry(ww::Shell &s, const char *name) {
  for (int i = 0; i < s.file_count; ++i)
    if (!std::strcmp(s.files[i].name, name))
      return i;
  assert(false);
  return 0;
}
void context(ww::Shell &s, const char *name) {
  const int index = entry(s, name);
  s.file_page = index / 10;
  const auto row = hit(s, 500 + index % 10);
  s.input_device = ww::InputDevice::mouse;
  s.pointer(row.x + row.w - 2, row.y + 4, false);
  s.open_context();
  assert(s.file_context && !s.context && s.file_selected == index);
}
void capture(ww::Shell &s,const char *path){
  s.draw();FILE *f=std::fopen(path,"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",ww::W,ww::H);
  for(int i=0;i<ww::W*ww::H;++i)assert(std::fwrite(s.pixels+i,1,3,f)==3);
  assert(std::fclose(f)==0);
}
void click(ww::Shell &s, int id) {
  const auto r = hit(s, id);
  s.pointer(r.x + r.w - 3, r.y + r.h - 3, true);
}
} // namespace
int main() {
  const auto root =
      fs::path("build") /
      ("context-fixture-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "Destination");
  const std::string original = "A real text file\n";
  put(root / "alpha.txt", original);
  ww::Shell s;
  assert(s.init("assets"));
  s.window = 1;
  s.file_open(root.string().c_str());
  context(s, "alpha.txt");
  const auto anchor = s.file_context_bounds;
  s.pointer(anchor.x + 20, anchor.y + 25, false);
  assert(s.file_context_bounds.x == anchor.x &&
         s.file_context_bounds.y == anchor.y);
  assert(
      has_hit(s, 802)); // Cut is a functional action.
  click(s, 804);
  assert(s.file_properties && !s.file_context &&
         s.file_properties_info.regular &&
         s.file_properties_info.size == original.size());
  s.activate(30);
  assert(s.pending == ww::Action::none && s.file_properties);
  click(s, 806);
  assert(!s.file_properties && s.window == 1);
  context(s, "alpha.txt");
  click(s, 801);
  assert(s.file_clipboard.ready && !s.file_context &&
         get(root / "alpha.txt") == original);
  // Background Paste goes into the current folder, choosing a new name.
  s.draw();
  s.pointer(s.file_content_bounds.x + s.file_content_bounds.w - 20,
            s.file_content_bounds.y + s.file_content_bounds.h - 100, false);
  s.open_context();
  assert(s.file_context && !s.file_context_item);
  click(s, 803);
  assert(s.file_transfer.busy());
  s.activate(93);
  assert(s.window == 1 && s.file_transfer.busy());
  for (int i = 0; s.file_transfer.busy() && i < 100; ++i)
    s.tick();
  assert(s.file_transfer.state == ww::fileops::State::complete);
  assert(get(root / "alpha - Copy.txt") == original &&
         get(root / "alpha.txt") == original);
  context(s, "Destination");
  assert(has_hit(s, 801));
  click(s, 804);
  assert(s.file_properties_info.directory);
  s.key(ww::Key::back);
  assert(!s.file_properties);
  context(s, "Destination");
  click(s, 803);
  for (int i = 0; s.file_transfer.busy() && i < 100; ++i)
    s.tick();
  assert(s.file_transfer.state == ww::fileops::State::complete &&
         get(root / "Destination" / "alpha.txt") == original);
  // Dismissing a context does not activate a control underneath it.
  context(s, "alpha.txt");
  s.pointer(20, 20, true);
  assert(!s.file_context && s.window == 1);
  s.pointer(20, 20, false);
  s.open_context();
  assert(s.context && s.desktop_context_target && !s.file_context && s.shortcut_context_id==10);
  s.key(ww::Key::back);
  assert(!s.context && !s.file_context);
  // Controller focus selects the row even when the pointer is elsewhere.
  const int index = entry(s, "alpha.txt");
  s.file_page = index / 10;
  s.draw();
  s.input_device = ww::InputDevice::controller;
  s.focus = 500 + index % 10;
  s.open_context();
  assert(s.file_context && s.file_selected == index && s.focus == 800);
  s.key(ww::Key::back);
  assert(!s.file_context);
  context(s, "alpha.txt");
  click(s, 800);
  assert(s.window == 4 && s.document.active &&
         std::string(s.document.text) == original);
  s.notepad_close();
  assert(s.window == 1);
  // Cancel is available while a larger transfer is visible and never deletes
  // its source or any pre-existing destination.
  put(root / "large.bin", std::string(ww::fileops::Transfer::Chunk * 4, 'x'));
  s.file_refresh();
  context(s, "large.bin");
  click(s, 801);
  context(s, "Destination");
  click(s, 803);
  assert(s.file_transfer.busy());
  click(s, 807);
  s.tick();
  assert(s.file_transfer.state == ww::fileops::State::cancelled && s.dialog);
  assert(fs::file_size(root / "large.bin") == ww::fileops::Transfer::Chunk * 4);
  assert(fs::exists(s.file_transfer.destination));
  s.activate(60);s.window=0;
  std::snprintf(s.preferences,sizeof(s.preferences),"%s/preferences",root.string().c_str());
  fs::create_directories(root/"Desktop");put(root/"Desktop"/"note.txt","Desktop test");
  s.desktop_files_next_scan=0;s.draw();
  auto desktop=hit(s,2000);s.pointer(desktop.x+5,desktop.y+5,false);s.open_context();
  assert(s.context&&s.desktop_context_target&&s.file_context_item);click(s,801);assert(s.file_clipboard.ready);
  s.draw();s.pointer(desktop.x+5,desktop.y+5,false);s.open_context();capture(s,"build/context-desktop-menu.ppm");click(s,809);
  assert(!fs::exists(root/"Desktop"/"note.txt")&&s.dialog);s.activate(60);
  s.draw();auto vapor=hit(s,35);s.pointer(vapor.x+5,vapor.y+5,false);s.open_context();
  assert(s.shortcut_context_id==35&&s.shortcut_context_taskbar);capture(s,"build/context-taskbar-menu.ppm");click(s,811);s.draw();
  for(int i=0;i<s.count;++i)assert(!(s.hits[i].id==35&&s.hits[i].r.y>=ww::H-54));
  ww::Shell reloaded;std::snprintf(reloaded.preferences,sizeof(reloaded.preferences),"%s",s.preferences);reloaded.load_preferences();
  assert(reloaded.taskbar_hidden & (1ULL<<35));assert(s.pending==ww::Action::none);
  // Reopening a deleted item's stale menu cannot delete a replacement.
  s.window=1;s.file_open(root.string().c_str());context(s,"alpha.txt");
  put(root/"alpha.txt","replacement");click(s,809);assert(get(root/"alpha.txt")=="replacement");
  s.activate(60);s.window=0;
  assert(s.file_clipboard.copy((root/"alpha.txt").string().c_str()));s.draw();
  s.pointer(600,300,false);s.open_context();assert(s.context);click(s,1409);
  for(int i=0;s.file_transfer.busy()&&i<100;++i)s.tick();
  assert(s.file_transfer.state==ww::fileops::State::complete && get(root/"Desktop"/"alpha.txt")=="replacement");
  s.window=1;s.file_open(root.string().c_str());context(s,"Destination");click(s,802);assert(s.file_clipboard.move);
  s.draw();s.pointer(s.file_content_bounds.x+s.file_content_bounds.w-20,s.file_content_bounds.y+s.file_content_bounds.h-100,false);s.open_context();click(s,803);
  for(int i=0;s.file_transfer.busy()&&i<1000;++i)s.tick();
  assert(s.file_transfer.state==ww::fileops::State::complete && !fs::exists(root/"Destination") && !s.file_clipboard.ready);
  std::puts(
      "PASS: anchored Explorer context, real Properties, Copy/Paste surfaces, "
      "controller focus, modal routing, progress/cancel and desktop isolation");
}
