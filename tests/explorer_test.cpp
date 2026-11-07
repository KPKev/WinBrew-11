#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
static ww::Rect hit(ww::Shell &s, int id, bool last = false) {
  s.draw();
  ww::Rect result{};
  bool found = false;
  for (int i = 0; i < s.count; ++i)
    if (s.hits[i].id == id) {
      result = s.hits[i].r;
      found = true;
      if (!last)
        break;
    }
  assert(found);
  return result;
}
int main() {
  ww::Shell s;
  assert(s.init("assets"));
  s.pointer(1000, 600, false);
  s.open_context();
  assert(s.context_x == 1000 && s.context_y == 600);
  s.pointer(1100, 650, false);
  assert(s.context_x == 1000 && s.context_y == 600);
  s.draw();
  s.pointer(20, 20, true);
  assert(!s.context);
  s.window = 1;
  std::snprintf(s.preferences, sizeof(s.preferences), "tests/settings.ini");
  // Home location cards are one surface, including their icon and empty edges.
  const int locations[] = {402, 403, 404, 401};
  for (int id : locations) {
    for (int area = 0; area < 3; ++area) {
      s.activate(400);
      const auto card = hit(s, id, true);
      const int px = area == 0   ? card.x + 2
                     : area == 1 ? card.x + 38
                                 : card.x + card.w - 2;
      s.pointer(px, card.y + card.h / 2, true);
      assert(s.file_location == id);
      assert(s.file_games == (id == 401));
      if (id != 401)
        assert(s.file_path[0]);
    }
  }
  // Library navigation selects a detected record without inventing a launch action.
  s.game_inventory.count = 10;
  for (int i = 0; i < 10; ++i) {
    s.game_inventory.entries[i].steam_app = 1000 + i;
    s.game_inventory.entries[i].state = ww::games::State::installed;
  }
  auto first_game = hit(s, 900);
  s.pointer(first_game.x + 10, first_game.y + 10, true);
  assert(s.library_selection == 0 && s.pending == ww::Action::none);
  auto next_page = hit(s, 968);
  s.pointer(next_page.x + 10, next_page.y + 10, true);
  assert(s.library_page == 1);
  auto later_game = hit(s, 908);
  s.pointer(later_game.x + 10, later_game.y + 10, true);
  assert(s.library_selection == 8);
  auto downloads = hit(s, 966);
  s.pointer(downloads.x + 10, downloads.y + 10, true);
  assert(s.library_downloads && s.pending == ww::Action::none);
  auto library = hit(s, 965);
  s.pointer(library.x + 10, library.y + 10, true);
  assert(!s.library_downloads && s.library_selection == 8);
  // The game library has its own navigation; return to File Explorer.
  s.activate(400);
  // Sidebar locations open on a single click, and Home remains reachable after
  // an unavailable location instead of trapping navigation behind an error.
  const int sidebar_locations[] = {402, 403, 404, 409, 400};
  for (int id : sidebar_locations) {
    const auto sidebar = hit(s, id);
    s.pointer(sidebar.x + 3, sidebar.y + 3, true);
    assert(s.file_location == id && !s.file_games);
  }
  s.activate(402);
  assert(!s.file_error && s.file_count > 0);
  s.activate(405);
  assert(!s.file_path[0] && s.file_location == 400);
  s.file_open("tests");
  s.draw();
  int row = -1;
  for (int i = 0; i < s.count; i++)
    if (s.hits[i].id == 500) {
      row = i;
      break;
    }
  assert(row >= 0);
  auto rect = s.hits[row].r;
  char before[1024];
  std::snprintf(before, sizeof(before), "%s", s.file_path);
  s.pointer(rect.x + 10, rect.y + 10, true, false, false);
  assert(s.focus == 500 && std::strcmp(before, s.file_path) == 0 &&
         !s.file_preview_open);
  assert(!s.file_error && s.file_count > 0);
  bool found = false;
  for (int i = 0; i < s.file_count; i++)
    if (!std::strcmp(s.files[i].name, "explorer_test.cpp")) {
      s.file_select(i);
      found = true;
      break;
    }
  assert(found && s.window == 4 && s.document.active &&
         std::strstr(s.document.text, "include"));
  s.notepad_close();
  assert(s.window == 1 && !s.document.active);
  s.file_refresh();
  // Drive the same double-click path as the mouse, through a real child folder.
  std::filesystem::create_directories("build/explorer-fixture/Child");
  FILE *fixture = std::fopen("build/explorer-fixture/Child/readme.txt", "wb");
  assert(fixture);
  std::fputs("Explorer preview fixture", fixture);
  std::fclose(fixture);
  std::snprintf(s.file_root, sizeof(s.file_root), "build/explorer-fixture");
  s.file_open(s.file_root);
  auto child = hit(s, 500);
  s.pointer(child.x + 20, child.y + 20, true, false, true);
  assert(!std::strcmp(s.file_path, "build/explorer-fixture/Child"));
  auto text_file = hit(s, 500);
  s.pointer(text_file.x + text_file.w - 2, text_file.y + 20, true, false, true);
  assert(s.window == 4 && s.document.active &&
         !std::strcmp(s.document.text, "Explorer preview fixture"));
  s.notepad_close();
  s.activate(405);
  assert(!s.document.active &&
         !std::strcmp(s.file_path, "build/explorer-fixture"));
  s.activate(405);
  assert(!s.file_path[0] && s.file_location == 400);
  s.file_open("build/no-such-explorer-test-dir");
  assert(s.file_error && !s.file_count &&
         !std::strcmp(s.file_error_stage, "Open folder"));
  s.draw();
  const auto home = hit(s, 400);
  s.pointer(home.x + 10, home.y + 10, true);
  assert(!s.file_path[0] && !s.file_error);
  s.activate(401);
  assert(s.file_games && s.pending == ww::Action::none);
  s.draw();
  std::puts("PASS: location-card surfaces, sidebar navigation, bounded Up, "
            "anchored context, directory listing, preview and error recovery");
}
