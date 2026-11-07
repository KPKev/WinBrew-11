// SPDX-License-Identifier: GPL-3.0-or-later
#include "cancel_registry_plan.hpp"
#include <string>
namespace ww::steam_integration {
CancelRegistryPlan PlanCanceledQueueRemoval(std::string_view registry, std::uint32_t app, bool record_absent) {
  if (!app || !record_absent || registry.size() > 65536 || registry.find('\0') != std::string_view::npos) return {};
  const auto key = std::to_string(app);
  std::string output;
  bool found = false;
  for (std::size_t pos = 0; pos < registry.size();) {
    const auto newline = registry.find('\n', pos);
    if (newline == std::string_view::npos) return {}; // Incomplete registry is not evidence.
    const auto line = registry.substr(pos, newline - pos);
    const auto tab = line.find('\t');
    if (line.substr(0, tab) == key) {
      if (found || tab == std::string_view::npos) return {};
      // This experiment is confined to the runtime's single internal volume.
      const auto fields = line.substr(tab + 1);
      if (fields != "internal\t0" && fields != "internal\t0\tpaused") return {};
      found = true;
    } else {
      output.append(line.data(), line.size()); output.push_back('\n');
    }
    pos = newline + 1;
  }
  return {found ? CancelRegistryPlanState::remove_queued : CancelRegistryPlanState::unchanged, std::move(output)};
}
}
