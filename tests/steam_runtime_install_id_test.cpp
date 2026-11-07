// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/runtime_install_id.hpp"
#include <cassert>
#include <cstdio>
int main() {
  using namespace ww::steam_integration;
  auto game = playden::GameId::Steam(1089130);
  assert(WineInstallId(game) == "wine11-steam-1089130");
  assert(ParseWineInstallId(WineInstallId(game)) == game);
  assert(!ParseWineInstallId("steam-1089130"));
  assert(!ParseWineInstallId("wine11-steam-01089130"));
  assert(!ParseWineInstallId("wine11-../steam-1089130"));
  assert(WineInstallId(playden::GameId{}).empty());
  std::puts("PASS: Wine 11 runtime game identities do not reuse Playden's bottle identities");
}
