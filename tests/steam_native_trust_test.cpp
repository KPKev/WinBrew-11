// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_trust.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv) {
  assert(argc == 2 && ww::steam_integration::VerifyPackagedSteamCa(argv[1]));
  char path[] = "/tmp/wine11-ca-test-XXXXXX"; assert(mkdtemp(path));
  std::filesystem::path root(path), copy = root / "ca.pem", alias = root / "link.pem";
  std::filesystem::copy_file(argv[1], copy);
  assert(ww::steam_integration::VerifyPackagedSteamCa(copy.c_str()));
  assert(symlink(copy.c_str(), alias.c_str()) == 0);
  assert(!ww::steam_integration::VerifyPackagedSteamCa(alias.c_str()));
  int fd = open(copy.c_str(), O_WRONLY); assert(fd >= 0);
  assert(write(fd, "!", 1) == 1 && close(fd) == 0);
  assert(!ww::steam_integration::VerifyPackagedSteamCa(copy.c_str()));
  std::filesystem::remove_all(root);
  std::puts("PASS: the tested CA is accepted; a symlink or altered CA is rejected");
}
