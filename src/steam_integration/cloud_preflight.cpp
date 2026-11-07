// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloud_preflight.hpp"
#include <utility>
namespace ww::steam_integration {
bool CloudPreflight::Begin(playden::steam::CloudLocation location, std::uint64_t account, std::uint64_t generation) {
  if (active_ || !location.id.steam_app() || !account || !generation || location.check_only ||
      location.game_root.empty() || location.game_root.front() != '/' ||
      location.state_root.empty() || location.state_root.front() != '/' ||
      location.prefix_root.empty() || location.prefix_root.front() != '/') return false;
  for (const auto& game : service_.Snapshot().games) {
    if (game.id != location.id) continue;
    if (game.report.queued || game.report.state == playden::steam::CloudState::kChecking ||
        game.report.state == playden::steam::CloudState::kSyncing) return false;
  }
  const auto serial = service_.Sync(location);
  if (!serial) return false;
  game_ = location.id; location_ = std::move(location); account_ = account; generation_ = generation; serial_ = serial;
  last_state_ = playden::steam::CloudState::kIdle; completed_serial_ = 0;
  active_ = true; invalidated_ = false; result_ = {CloudGateState::checking, "Checking Steam Cloud saves..."};
  return true;
}
bool CloudPreflight::Resolve(std::uint32_t app, std::uint64_t account, std::uint64_t generation,
                             playden::steam::CloudResolution choice, bool confirm_account) {
  using namespace playden::steam;
  if (active_ || invalidated_ || game_.steam_app() != app || account != account_ || generation != generation_ ||
      choice == CloudResolution::kAutomatic || !completed_serial_ ||
      (last_state_ != CloudState::kConflict && last_state_ != CloudState::kNeedsAccount) ||
      (last_state_ == CloudState::kNeedsAccount && !confirm_account)) return false;
  bool same_report = false;
  for (const auto& game : service_.Snapshot().games)
    if (game.id == game_ && game.report.serial == completed_serial_ && game.report.state == last_state_ && !game.report.queued)
      same_report = true;
  if (!same_report) return false;
  const auto serial = service_.Resolve(location_, choice, confirm_account);
  if (!serial) return false;
  serial_ = serial; completed_serial_ = 0; last_state_ = CloudState::kIdle;
  active_ = true; result_ = {CloudGateState::checking, "Resolving Steam Cloud saves..."};
  return true;
}
CloudGateResult CloudPreflight::Poll(std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
  if (game_.steam_app() != app || account != account_ || generation != generation_) invalidated_ = true;
  if (!active_) return invalidated_ ? CloudGateResult{CloudGateState::blocked, "Cloud check is no longer valid."} : result_;
  for (const auto& game : service_.Snapshot().games) {
    if (game.id != game_ || game.report.serial <= serial_) continue;
    using playden::steam::CloudState;
    switch (game.report.state) {
      case CloudState::kChecking: case CloudState::kSyncing: case CloudState::kIdle:
        return invalidated_ ? CloudGateResult{CloudGateState::blocked, "Steam account or game changed during the Cloud check."}
                            : CloudGateResult{CloudGateState::checking, "Checking Steam Cloud saves..."};
      case CloudState::kSynced: case CloudState::kUpToDate:
        result_ = invalidated_ ? CloudGateResult{CloudGateState::blocked, "Cloud check is no longer valid."}
                              : CloudGateResult{CloudGateState::ready, game.report.message};
        break;
      default:
        result_ = {CloudGateState::blocked, game.report.message.empty() ? "Steam Cloud needs attention before starting." : game.report.message};
        break;
    }
    last_state_ = game.report.state; completed_serial_ = game.report.serial;
    active_ = false; return result_;
  }
  return invalidated_ ? CloudGateResult{CloudGateState::blocked, "Steam account or game changed during the Cloud check."}
                      : CloudGateResult{CloudGateState::checking, "Checking Steam Cloud saves..."};
}
void CloudPreflight::Observe(std::uint64_t account, std::uint64_t generation) {
  if (active_) if (const auto app = game_.steam_app()) (void)Poll(*app, account, generation);
}
std::uint32_t CloudPreflight::ChoiceApp(std::uint64_t account, std::uint64_t generation) const {
  using playden::steam::CloudState;
  if (active_ || invalidated_ || account != account_ || generation != generation_ || !completed_serial_ ||
      (last_state_ != CloudState::kConflict && last_state_ != CloudState::kNeedsAccount)) return 0;
  return game_.steam_app().value_or(0);
}
std::uint32_t CloudPreflight::SkipApp(std::uint64_t account, std::uint64_t generation) const {
  if (active_ || invalidated_ || account != account_ || generation != generation_ || !completed_serial_ ||
      last_state_ != playden::steam::CloudState::kUnsupported) return 0;
  return game_.steam_app().value_or(0);
}
bool CloudPreflight::AllowWithoutCloud(std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
  if (!app || SkipApp(account, generation) != app) return false;
  bool same_report = false;
  for (const auto& game : service_.Snapshot().games)
    if (game.id == game_ && game.report.serial == completed_serial_ &&
        game.report.state == playden::steam::CloudState::kUnsupported && !game.report.queued) same_report = true;
  if (!same_report) return false;
  completed_serial_ = 0; last_state_ = playden::steam::CloudState::kIdle;
  result_ = {CloudGateState::ready, "Starting without Steam Cloud sync."};
  return true;
}
}
