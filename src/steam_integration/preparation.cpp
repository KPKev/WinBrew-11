// SPDX-License-Identifier: GPL-3.0-or-later
#include "preparation.hpp"
#include "backend_api.hpp"
#include <utility>
#include <cmath>
#include <algorithm>
namespace ww::steam_integration {
using namespace playden;
using namespace playden::homebrew;
namespace {
// Every game must refresh an older owned title before launching after a kit
// update. The runtime host preserves its bottle and scoped runtime selection.
bool SelectedRuntimeReady(const RuntimeInstallView& installed) {
  return installed.runtime_current && installed.title_current
#if WW_PLAYDEN_BACKEND_011
      && !installed.files_changed && !installed.republish_pending
#endif
      ;
}
}
Preparation::Preparation(PreparationHost& host) : host_(host), thread_(std::this_thread::get_id()) {}
bool Preparation::NeedsRegistration(std::uint32_t app) const {
  if (!app || std::this_thread::get_id() != thread_) return false;
  const auto current = host_.Snapshot();
  if (!current || !current->installs_known) return false;
  for (const auto& installed : current->installs)
    if (installed.game == GameId::Steam(app) && installed.runtime_number == app && installed.awaiting_registration) return true;
  return false;
}
bool Preparation::Request(RuntimePrepareRequest request, std::uint64_t account, std::uint64_t generation) {
  if (std::this_thread::get_id() != thread_ || Busy()) return false;
  view_ = {};
  auto fail = [this](const char* text) { view_ = {PreparationState::failed, text}; return false; };
  const auto id = request.game.steam_app();
  if (!id || *id != request.runtime_number || !account || !generation || request.install_path.empty() ||
      request.install_path.front() != '/' || request.volume_id.empty()) return fail("Game installation information is unavailable.");
  auto current = host_.Snapshot();
  if (!current || current->status != RuntimeHostStatus::kReady || !current->installs_known || !current->kit_available)
    return fail("Game runtime is not ready.");
  if (PrepareQueued(*current, *id) || current->preparing_app == *id)
    return fail("This game is already being prepared.");
  for (const auto& installed : current->installs) if (installed.game == request.game) {
    if (installed.awaiting_registration && installed.install_path == request.install_path)
      return fail("Close Vapor to finish PS5 registration, then reopen Vapor and press Play.");
    if (installed.runtime_number != *id || installed.moving || installed.state == runtime::PreparationState::kRemoving ||
        installed.install_path != request.install_path)
      return fail("This game's existing runtime needs attention before preparation.");
    // The runtime host resumes the saved preparation/publication journal and
    // checks registration before taking any new effects. Do not strand an
    // otherwise matching game merely because that journal exists.
    if (!installed.recovery_pending && installed.state == runtime::PreparationState::kReady && installed.admission == "ok" &&
        SelectedRuntimeReady(installed)) {
      view_ = {PreparationState::ready, "Game is ready to play."}; return true;
    }
  }
  app_ = *id; root_ = request.install_path; account_ = account; generation_ = generation;
  before_ = current->prepares_handled;
  account_changed_ = false; view_ = {PreparationState::waiting, "Preparing game..."};
  started_ = std::chrono::steady_clock::now();
  host_.Prepare(std::move(request));
  Tick(account, generation);
  return true;
}
void Preparation::Tick(std::uint64_t account, std::uint64_t generation) {
  if (std::this_thread::get_id() != thread_ || !Busy()) return;
  view_.elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - started_).count();
  view_.progress.reset();
  if (account != account_ || generation != generation_) account_changed_ = true;
  auto current = host_.Snapshot();
  if (!current || current->status != RuntimeHostStatus::kReady || !current->installs_known) {
    view_.message = "Waiting for game runtime status..."; return;
  }
  if (current->preparing_app == app_) {
    view_.step = "Setting up game";
    for (const auto& installed : current->installs) {
      if (installed.game != GameId::Steam(app_) || installed.install_path != root_ || installed.step.empty()) continue;
      view_.step = installed.step;
      for (char& c : view_.step) if (c == '-' || c == '_') c = ' ';
      if (view_.step[0] >= 'a' && view_.step[0] <= 'z') view_.step[0] -= 'a' - 'A';
      break;
    }
    if (current->work_progress && std::isfinite(*current->work_progress))
      view_.progress = std::clamp(*current->work_progress, 0.0f, 1.0f);
    return;
  }
  if (PrepareQueued(*current, app_)) { view_.step = "Queued for setup"; return; }
  for (const auto& installed : current->installs) {
    if (installed.game != GameId::Steam(app_) || installed.runtime_number != app_ || installed.install_path != root_) continue;
    if (installed.state == runtime::PreparationState::kReady && installed.admission == "ok" && SelectedRuntimeReady(installed) &&
        !installed.awaiting_registration && !installed.recovery_pending && !installed.moving) {
      view_ = account_changed_ ? PreparationView{PreparationState::failed, "Steam account changed while preparing the game."}
                               : PreparationView{PreparationState::ready, "Game is ready to play."}; return;
    }
    if (installed.awaiting_registration) {
      view_ = account_changed_ ? PreparationView{PreparationState::failed, "Steam account changed while preparing the game."}
                               : PreparationView{PreparationState::awaiting_registration, "Close Vapor to finish PS5 registration, then reopen Vapor and press Play."};
      return;
    }
  }
  if (current->prepares_handled <= before_) return;
  const auto refusal = current->refusals.find(app_);
  // A fast retry can finish before the first poll and repeat the same error.
  // Once it is no longer queued or active, the completed counter and this
  // game's refusal together identify the finished request.
  if (refusal != current->refusals.end())
    view_ = {PreparationState::failed, refusal->second.empty() ? "Game preparation failed." : refusal->second};
}
}
