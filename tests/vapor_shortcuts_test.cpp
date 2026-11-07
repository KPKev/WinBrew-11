#include "../src/vapor_shortcuts.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  const auto root = std::filesystem::temp_directory_path() /
      ("wine11-vapor-shortcuts-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  assert(std::filesystem::create_directory(root));
  const auto prefs = (root / "settings.txt").string();
  const auto path = root / "vapor-shortcuts.txt";
  ww::VaporShortcuts first; assert(first.load(prefs.c_str()));
  assert(first.toggle(976730, ww::VaporShortcuts::Desktop));
  assert(first.toggle(976730, ww::VaporShortcuts::Taskbar));
  ww::VaporShortcuts second; assert(second.load(prefs.c_str()));
  assert(second.count() == 1 && second.placements(976730) == 5);
  assert(second.toggle(976730, ww::VaporShortcuts::Desktop));
  ww::VaporShortcuts third; assert(third.load(prefs.c_str()) && third.placements(976730) == 4);
  assert(third.toggle(1, ww::VaporShortcuts::Taskbar));
  assert(third.toggle(2, ww::VaporShortcuts::Taskbar));
  assert(!third.toggle(3, ww::VaporShortcuts::Taskbar));
  ww::VaporShortcuts limited; assert(limited.load(prefs.c_str()) && limited.count() == 3);
  for (unsigned app = 100; app < 112; ++app) assert(limited.toggle(app, ww::VaporShortcuts::Favorite));
  ww::VaporShortcuts favorites; assert(favorites.load(prefs.c_str()));
  assert(favorites.count() == 15 && favorites.placements(111) == ww::VaporShortcuts::Favorite);
  { std::ofstream f(path, std::ios::binary | std::ios::trunc); f << "invalid data\n"; }
  ww::VaporShortcuts invalid; assert(!invalid.load(prefs.c_str()));
  assert(invalid.count() == 0 && !invalid.toggle(1, ww::VaporShortcuts::Start));
  std::ifstream f(path, std::ios::binary); std::string content((std::istreambuf_iterator<char>(f)), {});
  assert(content == "invalid data\n");
  f.close();
  std::filesystem::remove(path);
  std::filesystem::remove(root);
}
