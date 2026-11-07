// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/runtime_install_id.hpp"
#include "runtime/game_install.h"
#include "platform/storage.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
int main() {
  using namespace playden;
  using namespace playden::runtime;
  char path[] = "/tmp/wine11-runtime-id-XXXXXX"; assert(mkdtemp(path));
  Storage storage(path); assert(storage.EnsureRootDirectory().ok());
  RuntimeInstallRepository repository(storage, "runtime-installs.json", [](const std::string&) {
    return NativeTitleAvailability{TitleAvailability::kFree, TitleAvailability::kFree};
  });
  assert(repository.Load().ok());
  ReserveRequest request; request.game = GameId::Steam(1089130); request.runtime_number = 1089130;
  request.install_id = ww::steam_integration::WineInstallId(request.game);
  request.default_profile_id = "playden-steam-1089130"; request.runtime_bundle_id = "test-runtime";
  request.recipe_id = "steam-1089130-auto"; request.recipe_version = "1"; request.launch_entry_id = "0";
  request.launch_path = "game.exe"; request.install_path = "/private/wine11-games/1089130";
  request.volume_id = "internal"; request.kit_title_variants = {"FAKE78000"};
  auto result = repository.Reserve(request); assert(result.ok());
  assert(result.value().data_root == "/data/playden/games/wine11-steam-1089130");
  assert(repository.Find(request.install_id));
  assert(!repository.Find("steam-1089130"));
  RuntimeInstallRepository reopened(storage, "runtime-installs.json", [](const std::string&) {
    return NativeTitleAvailability{TitleAvailability::kOccupied, TitleAvailability::kOccupied};
  });
  assert(reopened.Load().ok());
  auto saved = reopened.Find(request.install_id);
  assert(saved && saved->game == request.game && saved->data_root == result.value().data_root);
  std::filesystem::remove_all(path);
  std::puts("PASS: Playden's real runtime repository accepts the Wine 11-specific bottle identity");
}
