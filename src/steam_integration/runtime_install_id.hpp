// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "library/store.h"
#include <optional>
#include <string>
#include <string_view>
namespace ww::steam_integration {
// Keep Wine 11's game bottles distinct from Playden's existing steam-<app> bottles.
std::string WineInstallId(const playden::GameId& game);
std::optional<playden::GameId> ParseWineInstallId(std::string_view id);
}
