// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace ww::steam_integration {
// Pure planning only: no registry write or deletion is authorized here.
enum class CancelRegistryPlanState { blocked, unchanged, remove_queued };
struct CancelRegistryPlan { CancelRegistryPlanState state = CancelRegistryPlanState::blocked; std::string contents; };
// `record_absent` must be established separately while the install worker is
// stopped. False includes both an existing completed install and uncertainty.
CancelRegistryPlan PlanCanceledQueueRemoval(std::string_view registry, std::uint32_t app, bool record_absent);
}
