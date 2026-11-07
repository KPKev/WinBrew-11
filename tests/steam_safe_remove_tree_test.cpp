#include "../src/steam_integration/safe_remove_tree.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdlib>
using namespace ww::steam_integration;
int main() {
  char temp[] = "/tmp/vapor-remove-XXXXXX"; assert(mkdtemp(temp));
  const auto base = std::filesystem::path(temp);
  std::filesystem::create_directories(base / "games" / "123" / "nested");
  std::filesystem::create_directories(base / "other");
  std::ofstream(base / "games" / "123" / "nested" / "file") << "data";
  std::ofstream(base / "other" / "keep") << "keep";
  assert(symlink((base / "other").c_str(), (base / "games" / "123" / "outside").c_str()) == 0);
  int parent = open((base / "games").c_str(), O_RDONLY | O_DIRECTORY); assert(parent >= 0);
  struct stat st{}; assert(fstatat(parent, "123", &st, AT_SYMLINK_NOFOLLOW) == 0);
  RemoveIdentity expected{st.st_dev, st.st_ino, st.st_uid};
  auto wrong = expected; ++wrong.inode;
  assert(!RemoveOwnedTree(parent, "123", wrong));
  assert(std::filesystem::exists(base / "games" / "123" / "nested" / "file"));
  assert(!RemoveOwnedTree(parent, "../other", expected));
  assert(RemoveOwnedTree(parent, "123", expected));
  assert(!std::filesystem::exists(base / "games" / "123"));
  assert(std::filesystem::exists(base / "other" / "keep"));
  assert(symlink((base / "other").c_str(), (base / "games" / "alias").c_str()) == 0);
  assert(!RemoveOwnedTree(parent, "alias", expected));
  assert(std::filesystem::exists(base / "other" / "keep"));
  std::filesystem::create_directory(base / "games" / "special");
  assert(mkfifo((base / "games" / "special" / "pipe").c_str(), 0600) == 0);
  assert(fstatat(parent, "special", &st, AT_SYMLINK_NOFOLLOW) == 0);
  RemoveIdentity special{st.st_dev, st.st_ino, st.st_uid};
  assert(!RemoveOwnedTree(parent, "special", special));
  assert(std::filesystem::exists(base / "games" / "special" / "pipe"));
  close(parent); std::filesystem::remove_all(base);
}
