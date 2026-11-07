// SPDX-License-Identifier: GPL-3.0-or-later
#include "ready_launch.hpp"
#include <utility>
namespace ww::steam_integration {
using namespace playden;
using namespace playden::homebrew;
ReadyLaunch::ReadyLaunch(ReadyLaunchHost& host, CloudGate gate, CloudBegin begin)
    : host_(host), cloud_(std::move(gate)), begin_cloud_(std::move(begin)), thread_(std::this_thread::get_id()) {}
bool ReadyLaunch::ApplicationThread() const { return thread_ == std::this_thread::get_id(); }
bool ReadyLaunch::Busy() const {
  return view_.state == ReadyLaunchState::waiting || view_.state == ReadyLaunchState::handoff ||
         view_.state == ReadyLaunchState::running;
}
bool ReadyLaunch::Request(std::uint32_t app, const std::string& root, std::uint64_t account, std::uint64_t generation) {
  return RequestImpl(app, root, account, generation, true);
}
bool ReadyLaunch::RequestUsingCompletedCloud(std::uint32_t app, const std::string& root, std::uint64_t account, std::uint64_t generation) {
  return RequestImpl(app, root, account, generation, false);
}
bool ReadyLaunch::RequestImpl(std::uint32_t app, const std::string& root, std::uint64_t account, std::uint64_t generation, bool begin_cloud) {
  if (!ApplicationThread() || Busy()) return false;
  view_ = {}; view_.app = app; submitted_ = handoff_attempted_ = cloud_pending_ = false;
  auto fail = [this](const char* reason) { view_.state = ReadyLaunchState::failed; view_.message = reason; return false; };
  if (!app || !account || !generation || root.empty() || root.front() != '/') return fail("The installed game location is unavailable.");
  auto current = host_.Snapshot();
  if (!current || current->status != RuntimeHostStatus::kReady || !current->installs_known || !current->kit_available)
    return fail("Game runtime is not ready.");
  if (current->handoff_ready || (current->launch &&
      current->launch->state != runtime::LaunchState::kIdle &&
      current->launch->state != runtime::LaunchState::kExited &&
      current->launch->state != runtime::LaunchState::kFailed))
    return fail("Another game launch is already in progress.");
  const auto id = GameId::Steam(app);
  const RuntimeInstallView* found = nullptr;
  for (const auto& record : current->installs)
    if (record.game == id && record.runtime_number == app) { found = &record; break; }
  if (!found || found->state != runtime::PreparationState::kReady ||
      found->install_path != root || found->awaiting_registration || found->recovery_pending ||
      found->moving || found->admission != "ok")
    return fail("This game needs a verified prepared title before it can start.");
  if (!cloud_) return fail("Cloud-save preflight is unavailable.");
  if (begin_cloud && begin_cloud_) {
    std::string error;
    if (!begin_cloud_(app, root, account, generation, &error)) {
      view_.state = ReadyLaunchState::failed;
      view_.message = error.empty() ? "Cloud-save preflight could not start." : std::move(error);
      return false;
    }
  }
  const auto cloud = cloud_(app, account, generation);
  if (cloud.state == CloudGateState::blocked) {
    view_.state = ReadyLaunchState::failed;
    view_.message = cloud.message.empty() ? "Cloud-save preflight has not completed." : cloud.message;
    return false;
  }
  account_id_ = account; account_generation_ = generation; installed_root_ = root;
  if (cloud.state == CloudGateState::checking) {
    cloud_pending_ = true; view_.state = ReadyLaunchState::waiting;
    view_.message = cloud.message.empty() ? "Checking Steam Cloud saves..." : cloud.message;
    return true;
  }
  plays_before_ = current->plays_handled;
  view_.state = ReadyLaunchState::waiting; view_.message = "Preparing to start...";
  submitted_ = true;
  host_.Play(app);
  return true;
}
void ReadyLaunch::Tick(std::uint64_t account, std::uint64_t generation) {
  if (!ApplicationThread() || !Busy()) return;
  const bool same_account = account == account_id_ && generation == account_generation_;
  if (cloud_pending_) {
    if (!same_account) { view_.state = ReadyLaunchState::failed; view_.message = "Steam account changed before starting."; cloud_pending_ = false; return; }
    const auto current = host_.Snapshot();
    if (!current || current->status != RuntimeHostStatus::kReady || !current->installs_known || !current->kit_available) {
      view_.message = "Waiting for game runtime status..."; return;
    }
    const RuntimeInstallView* found = nullptr;
    for (const auto& record : current->installs)
      if (record.game == GameId::Steam(view_.app) && record.runtime_number == view_.app) { found = &record; break; }
    if (!found || found->install_path != installed_root_ || found->state != runtime::PreparationState::kReady ||
        found->awaiting_registration || found->recovery_pending || found->moving || found->admission != "ok") {
      view_.state = ReadyLaunchState::failed; view_.message = "The prepared game changed before starting."; cloud_pending_ = false; return;
    }
    if (current->handoff_ready || (current->launch && current->launch->state != runtime::LaunchState::kIdle &&
        current->launch->state != runtime::LaunchState::kExited && current->launch->state != runtime::LaunchState::kFailed)) {
      view_.state = ReadyLaunchState::failed; view_.message = "Another game launch is already in progress."; cloud_pending_ = false; return;
    }
    const auto cloud = cloud_(view_.app, account_id_, account_generation_);
    if (cloud.state == CloudGateState::checking) { view_.message = cloud.message.empty() ? "Checking Steam Cloud saves..." : cloud.message; return; }
    if (cloud.state != CloudGateState::ready) {
      view_.state = ReadyLaunchState::failed; view_.message = cloud.message.empty() ? "Cloud-save preflight has not completed." : cloud.message; cloud_pending_ = false; return;
    }
    cloud_pending_ = false; submitted_ = true; plays_before_ = current->plays_handled;
    view_.message = "Preparing to start..."; host_.Play(view_.app); return;
  }
  if (!submitted_) return;
  auto current = host_.Snapshot();
  if (!current || current->status != RuntimeHostStatus::kReady || !current->installs_known) {
    view_.state = ReadyLaunchState::waiting;
    view_.message = "Game runtime status is unavailable; launch remains on hold."; return;
  }
  if (current->plays_handled <= plays_before_) return;
  const auto refusal = current->play_refusals.find(view_.app);
  if (refusal != current->play_refusals.end()) {
    view_.state = ReadyLaunchState::failed;
    view_.message = refusal->second.empty() ? "The game could not start." : refusal->second;
    return;
  }
  if (!current->launch || current->launch->game != GameId::Steam(view_.app)) return;
  const auto& launch = *current->launch;
  if (!same_account && launch.state != runtime::LaunchState::kExited &&
      launch.state != runtime::LaunchState::kFailed) {
    view_.message = "Steam account changed; launch is on hold."; return;
  }
  switch (launch.state) {
  case runtime::LaunchState::kReadyToHandoff:
    if (handoff_attempted_ || !current->handoff_ready) return;
    if (!cloud_ || cloud_(view_.app, account_id_, account_generation_).state != CloudGateState::ready) {
      view_.state = ReadyLaunchState::waiting;
      view_.message = "Cloud-save status changed; waiting before starting.";
      return;
    }
    handoff_attempted_ = true;
    if (!host_.Handoff()) {
      view_.state = ReadyLaunchState::waiting;
      view_.message = "Game handoff status is uncertain; waiting for the runtime."; return;
    }
    view_.state = ReadyLaunchState::handoff; view_.message = "Starting game..."; break;
  case runtime::LaunchState::kLaunching: view_.state = ReadyLaunchState::handoff; view_.message = "Starting game..."; break;
  case runtime::LaunchState::kRunning: view_.state = ReadyLaunchState::running; view_.message = "Game is running."; break;
  case runtime::LaunchState::kExited: view_.state = ReadyLaunchState::exited; view_.message = "Game exited."; break;
  case runtime::LaunchState::kFailed:
    view_.state = ReadyLaunchState::failed;
    view_.message = launch.message.empty() ? "The game could not start." : launch.message; break;
  default: break;
  }
}
}
