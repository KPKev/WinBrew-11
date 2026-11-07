// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloud_launch_gate.hpp"
#include <utility>
namespace ww::steam_integration {
CloudLaunchGate::CloudLaunchGate(CloudPreflight& preflight, CloudLocationResolver resolver)
    : preflight_(preflight), resolver_(std::move(resolver)) {}
bool CloudLaunchGate::Begin(std::uint32_t app, const std::string& root, std::uint64_t account,
                            std::uint64_t generation, std::string* error) {
  if (error) error->clear();
  auto fail = [error](const char* message) { if (error) *error = message; return false; };
  if (!resolver_) return fail("Cloud-save locations are unavailable.");
  auto location = resolver_(app);
  if (!location || location->id.steam_app() != app || location->game_root != root)
    return fail("Cloud-save locations do not match the installed game.");
  if (!preflight_.Begin(std::move(*location), account, generation))
    return fail("Cloud-save check could not start.");
  return true;
}
CloudGateResult CloudLaunchGate::Poll(std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
  return preflight_.Poll(app, account, generation);
}
}
