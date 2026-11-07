// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_install_id.hpp"
namespace ww::steam_integration {
std::string WineInstallId(const playden::GameId& game) {
  if (!game.valid()) return {};
  return "wine11-" + playden::ToInstallId(game);
}
std::optional<playden::GameId> ParseWineInstallId(std::string_view id) {
  constexpr std::string_view prefix = "wine11-";
  if (id.substr(0, prefix.size()) != prefix) return std::nullopt;
  auto game = playden::ParseInstallId(id.substr(prefix.size()));
  if (!game || WineInstallId(*game) != id) return std::nullopt;
  return game;
}
}
