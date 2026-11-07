// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native_bridge.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ww::steam_integration {
enum class GameStartupState { idle, preparing, ready, failed };
// Owns the bridge and its game-title coordination for the entire app lifetime.
// The provision callback runs on a worker. Tick and all launch callbacks run
// on the application thread. The supplied platform callbacks must outlive it.
class NativeGameStartup {
 public:
  using Provision = std::function<std::unique_ptr<CoordinatedTitleList>()>;
  explicit NativeGameStartup(ww::Shell& shell);
  ~NativeGameStartup();
  NativeGameStartup(const NativeGameStartup&) = delete;
  NativeGameStartup& operator=(const NativeGameStartup&) = delete;
  bool Start(const char* state_root, int anchor, const char* ca, std::string runtime_dir,
             Provision provision, playden::ps5::TitleRegistry::Transport registry,
             NativeTitleLauncher launcher, int game_data_anchor = -1);
  GameStartupState Tick();
 private:
  std::mutex mutex_;
  std::thread worker_;
  std::unique_ptr<CoordinatedTitleList> pending_, titles_;
  // Explicit destruction order: host/bridge must stop before title lease.
  std::unique_ptr<NativeBridge> bridge_;
  std::string runtime_dir_;
  playden::ps5::TitleRegistry::Transport registry_;
  NativeTitleLauncher launcher_;
  Provision provision_;
  GameStartupState state_ = GameStartupState::idle;
  bool worker_started_ = false, worker_done_ = false;
};
}
