// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/cloud_location.hpp"
#include <cassert>
int main() {
  using namespace playden;
  using namespace ww::steam_integration;
  homebrew::RuntimeSnapshot snapshot;
  snapshot.status = homebrew::RuntimeHostStatus::kReady;
  snapshot.kit_available = true;
  homebrew::RuntimeInstallView game;
  game.game = GameId::Steam(42); game.runtime_number = 42;
  game.state = runtime::PreparationState::kReady; game.admission = "ok";
  game.install_path = "/games/42"; game.prefix_root = "/bottles/42/prefix";
  snapshot.installs.push_back(game);
  auto location = CloudLocationFor(snapshot, 42, "/private/cloud-saves");
  assert(location && location->game_root == game.install_path && location->prefix_root == game.prefix_root);
  assert(location->state_root == "/private/cloud-saves/" + ToKey(game.game) && !location->check_only);
  assert(!CloudLocationFor(snapshot, 43, "/private/cloud-saves"));
  assert(!CloudLocationFor(snapshot, 42, "/private/../unsafe"));
  snapshot.installs.push_back(game);
  assert(!CloudLocationFor(snapshot, 42, "/private/cloud-saves"));
  snapshot.installs.pop_back();
  snapshot.installs[0].prefix_root = "/bottles/../other";
  assert(!CloudLocationFor(snapshot, 42, "/private/cloud-saves"));
  snapshot.installs[0] = game; snapshot.installs[0].awaiting_registration = true;
  assert(!CloudLocationFor(snapshot, 42, "/private/cloud-saves"));
  snapshot.installs[0] = game; snapshot.kit_available = false;
  assert(!CloudLocationFor(snapshot, 42, "/private/cloud-saves"));
}
