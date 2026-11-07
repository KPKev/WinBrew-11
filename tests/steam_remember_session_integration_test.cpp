// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_paths.hpp"
#include "../src/steam_integration/remember_setting.hpp"
#include "steam/local_session_repository.h"
#include "platform/ps5/title_allow_list.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
int main() {
  char path[] = "/tmp/wine11-remember-XXXXXX"; assert(mkdtemp(path));
  int anchor = open(path, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  auto paths = ww::steam_integration::PrepareNativePaths(path, anchor); assert(paths);
  assert(ww::steam_integration::HasSavedSteamSession(*paths) == false);
  {
    playden::SessionVault vault(false);
    playden::LocalSessionRepository repo(paths->state.path().string(), {}, &vault);
    ww::steam_integration::RememberSetting setting(false, [&](bool enabled) {
      const auto uid = playden::ps5::HoldEffectiveUid(); return repo.SetPersist(enabled).ok();
    });
    assert(setting.Set(true));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (setting.View().busy && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(setting.View().enabled && !setting.View().failed);
    assert(repo.Save(playden::StoredSession{42,"synthetic","refresh","access",{}}).ok());
    assert(ww::steam_integration::HasSavedSteamSession(*paths) == true);
  }
  {
    playden::SessionVault restored(true);
    playden::LocalSessionRepository repo(paths->state.path().string(), {}, &restored);
    const auto session = repo.Load(); assert(session.ok() && session.value() && session.value()->steam_id == 42);
    assert(repo.Clear().ok());
    assert(ww::steam_integration::HasSavedSteamSession(*paths) == false);
  }
  paths.reset(); close(anchor); std::filesystem::remove_all(path);
}
