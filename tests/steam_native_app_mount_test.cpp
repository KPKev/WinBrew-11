// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_app_mount.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <unistd.h>
int main() {
  char path[] = "/tmp/wine11-app-mount-XXXXXX"; assert(mkdtemp(path));
  std::filesystem::path root(path), alias = root / "alias";
  assert(symlink(path, alias.c_str()) == 0);
  int anchor = open(path, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  const char* bad[] = {"/tmp", alias.c_str()};
  char out[1024];
  assert(!ww::steam_integration::ResolvePackagedCa(anchor, bad, 2, out, sizeof(out)) && !out[0]);
  const char* good[] = {"/tmp", path};
  assert(ww::steam_integration::ResolvePackagedCa(anchor, good, 2, out, sizeof(out)));
  assert(std::string(out) == std::string(path) + "/assets/steam/cacert.pem");
  assert(!ww::steam_integration::ResolvePackagedCa(anchor, good, 2, out, 4) && !out[0]);
  close(anchor); std::filesystem::remove_all(root);
  std::puts("PASS: packaged Steam CA path binds to the anchored app mount");
}
