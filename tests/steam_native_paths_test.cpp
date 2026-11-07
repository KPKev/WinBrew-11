// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_paths.hpp"
#include "../src/steam_integration/cancel_recovery_capability.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  char path[] = "/tmp/wine11-native-paths-XXXXXX";
  assert(mkdtemp(path));
  const std::filesystem::path root(path);
  int anchor = open(path, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto first = ww::steam_integration::PrepareNativePaths(path, anchor); assert(first);
    assert(first->state.path() == root / "wine11-steam");
    ww::steam_integration::CancelRecoveryCapability recovery(first->state, first->state_lease);
    assert(recovery.Valid() && !recovery.BeforeScan());
    const auto state_lock = root / "wine11-steam/wine11-state.lock";
    const auto renamed_lock = root / "wine11-steam/wine11-state.lock.test";
    assert(rename(state_lock.c_str(), renamed_lock.c_str()) == 0);
    assert(!recovery.Valid() && !recovery.BeforeScan());
    assert(rename(renamed_lock.c_str(), state_lock.c_str()) == 0);
    assert(recovery.Valid() && !recovery.BeforeScan());
    assert(first->games.path() == root / "wine11-steam/wine11-games");
    assert(!ww::steam_integration::PrepareNativePaths(path, anchor));
    struct stat st{};
    assert(stat((root / "wine11-steam/wine11-games/wine11-install.lock").c_str(), &st) == 0);
    assert(S_ISREG(st.st_mode) && (st.st_mode & 07777) == 0600);
    assert(stat((root / "wine11-steam").c_str(), &st) == 0 && (st.st_mode & 07777) == 0700);
    assert(ww::steam_integration::HasSavedSteamSession(*first) == false);
    const auto session = root / "wine11-steam/session.v1";
    assert(symlink("/etc/passwd", session.c_str()) == 0);
    assert(!ww::steam_integration::HasSavedSteamSession(*first));
    assert(unlink(session.c_str()) == 0);
    int saved = open(session.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(saved >= 0);
    assert(write(saved, "x", 1) == 1 && close(saved) == 0);
    assert(ww::steam_integration::HasSavedSteamSession(*first) == true);
    assert(chmod(session.c_str(), 0644) == 0);
    assert(!ww::steam_integration::HasSavedSteamSession(*first));
    assert(unlink(session.c_str()) == 0);
  }
  assert(ww::steam_integration::PrepareNativePaths(path, anchor));
  const auto other = root / "other"; assert(mkdir(other.c_str(), 0700) == 0);
  assert(!ww::steam_integration::PrepareNativePaths(other.c_str(), anchor));
  const auto alias = root / "alias"; assert(symlink(path, alias.c_str()) == 0);
  assert(!ww::steam_integration::PrepareNativePaths(alias.c_str(), anchor));
  if (geteuid() == 0) {
    const auto data = root / "data", wine = data / "wine11";
    assert(mkdir(data.c_str(), 0755) == 0 && mkdir(wine.c_str(), 0755) == 0);
    int global_anchor = open(data.c_str(), O_RDONLY | O_DIRECTORY); assert(global_anchor >= 0);
    assert(!ww::steam_integration::PrepareNativePaths(path, anchor, anchor, data.c_str()));
    {
      auto main_drive = ww::steam_integration::PrepareNativePaths(path, anchor, global_anchor, data.c_str());
      assert(main_drive && main_drive->games.path() == wine / "wine11-games");
      struct stat st{}; assert(stat(main_drive->games.path().c_str(), &st) == 0 && (st.st_mode & 07777) == 0700);
      assert(std::filesystem::exists(root / "wine11-steam/wine11-games/wine11-install.lock"));
    }
    assert(chmod(wine.c_str(), 0777) == 0);
    assert(!ww::steam_integration::PrepareNativePaths(path, anchor, global_anchor, data.c_str()));
    assert(close(global_anchor) == 0);
  }
  assert(close(anchor) == 0);
  std::filesystem::remove_all(root);
  std::puts("PASS: native Steam paths are anchored, private and protected by an exclusive state lease");
}
