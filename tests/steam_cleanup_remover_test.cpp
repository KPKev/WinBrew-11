#include "steam/backend/depot_installer.h"
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static std::string denied;
extern "C" int __real_lstat(const char*, struct stat*);
extern "C" int __wrap_lstat(const char* path, struct stat* info) {
  if (denied == path) { errno = EACCES; return -1; }
  return __real_lstat(path, info);
}

int main() {
  char root[] = "/tmp/wine11-remover-test-XXXXXX";
  assert(mkdtemp(root));
  const std::string game = std::string(root) + "/440";
  std::string failure;
  assert(playden::steam::RemoveManagedInstall(game, &failure));
  denied = game;
  assert(!playden::steam::RemoveManagedInstall(game, &failure));
  assert(failure.find("inspected") != std::string::npos);
  denied.clear();
  assert(std::filesystem::create_directory(game));
  assert(!playden::steam::RemoveManagedInstall(game, &failure));
  assert(std::filesystem::exists(game)); // Markerless directories stay untouched.
  assert(std::filesystem::remove(game));
  assert(symlink(root, game.c_str()) == 0);
  assert(!playden::steam::RemoveManagedInstall(game, &failure));
  assert(std::filesystem::is_symlink(game));
  assert(unlink(game.c_str()) == 0);
  assert(rmdir(root) == 0);
}
