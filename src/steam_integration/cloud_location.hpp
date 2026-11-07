// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include "steam/cloud_service.h"
#include <filesystem>
#include <optional>

namespace ww::steam_integration {
// Select only a single admitted, fully prepared record. The caller owns and
// validates the private saves directory for the lifetime of Cloud operations.
inline std::optional<playden::steam::CloudLocation> CloudLocationFor(
    const playden::homebrew::RuntimeSnapshot& snapshot, std::uint32_t app,
    const std::filesystem::path& saves) {
  using namespace playden;
  if (!app || !saves.is_absolute() || saves != saves.lexically_normal() ||
      snapshot.status != homebrew::RuntimeHostStatus::kReady || !snapshot.installs_known || !snapshot.kit_available)
    return std::nullopt;
  const homebrew::RuntimeInstallView* found = nullptr;
  for (const auto& item : snapshot.installs) {
    if (item.game != GameId::Steam(app)) continue;
    if (found) return std::nullopt;
    found = &item;
  }
  if (!found || found->runtime_number != app || found->state != runtime::PreparationState::kReady ||
      found->admission != "ok" || found->awaiting_registration || found->recovery_pending || found->moving)
    return std::nullopt;
  const std::filesystem::path game(found->install_path), prefix(found->prefix_root);
  if (!game.is_absolute() || game != game.lexically_normal() ||
      !prefix.is_absolute() || prefix != prefix.lexically_normal()) return std::nullopt;
  return steam::CloudLocation{GameId::Steam(app), game.string(),
                              (saves / ToKey(GameId::Steam(app))).string(), prefix.string(), false};
}
}
