// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_game_startup.hpp"
#include <utility>
namespace ww::steam_integration {
NativeGameStartup::NativeGameStartup(ww::Shell& shell) : bridge_(std::make_unique<NativeBridge>(shell)) {}
NativeGameStartup::~NativeGameStartup() {
  // Stop account/download workers first: a transfer may hold the UID guard
  // that the independent title-list worker is waiting to acquire.
  bridge_.reset();
  if (worker_.joinable()) worker_.join();
}
bool NativeGameStartup::Start(const char* root, int anchor, const char* ca, std::string runtime,
                              Provision provision, playden::ps5::TitleRegistry::Transport registry,
                              NativeTitleLauncher launcher, int game_data_anchor) {
  if (state_ != GameStartupState::idle || runtime.empty() || runtime.front() != '/' ||
      !registry || !launcher || !bridge_->Start(root, anchor, ca, game_data_anchor)) return false;
  // Account and library remain available if the game-host prerequisites are
  // missing. Tick still attaches the independently started account UI.
  if (!provision) { state_ = GameStartupState::failed; return true; }
  runtime_dir_ = std::move(runtime); registry_ = std::move(registry); launcher_ = std::move(launcher);
  provision_ = std::move(provision);
  state_ = GameStartupState::preparing;
  return true;
}
GameStartupState NativeGameStartup::Tick() {
  const auto startup = bridge_->Tick();
  if (state_ != GameStartupState::preparing) return state_;
  if (startup == NativeStartupState::failed) return state_ = GameStartupState::failed;
  if (startup != NativeStartupState::ready) return state_;
  if (!worker_started_) {
    // Do not elevate on the title worker until private account storage has
    // finished initializing: the effective UID is process-wide.
    try {
      worker_ = std::thread([this, provision = std::move(provision_)] {
        std::unique_ptr<CoordinatedTitleList> result;
        try { result = provision(); } catch (...) { }
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = std::move(result); worker_done_ = true;
      });
      worker_started_ = true;
    } catch (...) { return state_ = GameStartupState::failed; }
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!worker_done_) return state_;
    titles_ = std::move(pending_);
  }
  if (!titles_ || !bridge_->StartGameHost(runtime_dir_, *titles_, registry_, launcher_, {}))
    return state_ = GameStartupState::failed;
  return state_ = GameStartupState::ready;
}
}
