#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
static ww::Rect hit(ww::Shell &s, int id) {
  s.draw();
  for (int i=0; i<s.count; ++i) if (s.hits[i].id==id) return s.hits[i].r;
  assert(false); return {};
}
int main() {
  std::filesystem::create_directories("build/history-ui-fixture");
  const char *path="build/history-ui-fixture/recent-websites.txt";
  std::remove(path);
  ww::Shell s;
  assert(s.init("assets",nullptr,nullptr,nullptr,"build/history-ui-fixture/settings.ini"));
  assert(s.web_history.remember("https://example.com/private?token=secret"));
  s.window=3;
  auto row=hit(s,850);
  s.pointer(row.x+8,row.y+8,true);
  assert(s.pending==ww::Action::browser && !std::strcmp(s.url,"https://example.com"));
  s.pending=ww::Action::none;
  auto clear=hit(s,855);
  s.pointer(clear.x+8,clear.y+8,true);
  assert(s.web_history.count==0 && !s.web_history.dirty);
  ww::WebHistory reloaded;
  assert(reloaded.load(path) && reloaded.count==0);
  std::remove(path);
  std::puts("PASS: recent-site activation and persistent clear");
}
