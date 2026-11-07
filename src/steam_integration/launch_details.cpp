// SPDX-License-Identifier: GPL-3.0-or-later
#include "launch_details.hpp"
#include <utility>
namespace ww::steam_integration {
bool LaunchDetails::Request(playden::homebrew::RuntimePrepareRequest request, std::uint64_t account, std::uint64_t generation) {
  if (state_ == LaunchDetailsState::waiting) return false;
  const auto app = request.game.steam_app();
  if (!app || *app != request.runtime_number || !account || !generation) return false;
  request_ = std::move(request); account_ = account; generation_ = generation;
  request_.launches.clear(); request_.launch_reason.clear(); request_.launches_any_branch = false;
  state_ = LaunchDetailsState::waiting; message_ = "Getting game launch options from Steam...";
  // No other consumer shares this service, so clear stale answers and permit a retry.
  service_.Invalidate(generation_);
  service_.Request(request_.game, generation_);
  return true;
}
void LaunchDetails::Tick(std::uint64_t account, std::uint64_t generation) {
  if (state_ != LaunchDetailsState::waiting) return;
  if (account != account_ || generation != generation_) {
    service_.Invalidate(generation);
    state_ = LaunchDetailsState::failed; message_ = "Steam account changed while loading game details."; return;
  }
  for (const auto& details : service_.Pump(8)) {
    if (details.id != request_.game || details.account_generation != generation_) continue;
    for (const auto& option : details.launches)
      request_.launches.push_back({"", option.description, option.executable, option.working_directory, option.arguments});
    request_.launches_any_branch = details.launches_any_branch;
    if (!details.available || details.launches.empty()) {
      request_.launch_reason = details.reason.empty() ? "Steam did not provide launch options for this game." : details.reason;
    }
    state_ = LaunchDetailsState::ready; message_.clear(); return;
  }
}
void LaunchDetails::Cancel() {
  if (state_ != LaunchDetailsState::waiting) return;
  service_.Invalidate(generation_);
  state_ = LaunchDetailsState::failed; message_ = "Game details request canceled.";
}
}
