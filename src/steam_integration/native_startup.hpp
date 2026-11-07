// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "ready_launch.hpp"
#include "cloud_launch_gate.hpp"
#include "native_host_config.hpp"

namespace ww { class SteamUi; }
namespace ww::steam_integration {
class NativeSession;
class CloudPreflight;
enum class NativeStartupState { idle, starting, ready, failed };

// Start after elevation, then poll only on the application thread. The owner
// must clear any borrowed UI pointer before destroying this object.
class NativeStartup {
 public:
  NativeStartup();
  ~NativeStartup();
  NativeStartup(const NativeStartup&) = delete;
  NativeStartup& operator=(const NativeStartup&) = delete;
  // ca_bundle must be a verified, immutable packaged file. The anchor is duped;
  // the caller keeps ownership of its original descriptor.
  bool Start(const char* rebound_root, int anchored_fd, const char* ca_bundle, int game_data_anchor = -1);
  NativeStartupState Poll();
  ww::SteamUi* ui();
  bool AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud);
  CloudPreflight* EnableCloud();
  bool AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver);
  CloudLocationResolver CloudLocations(playden::homebrew::RuntimeHost& host);
  std::optional<playden::homebrew::RuntimeHostConfig> GameHostConfig(
      std::string packaged_runtime_dir, CoordinatedTitleList& titles,
      playden::ps5::TitleRegistry::Transport registry, NativeTitleLauncher launcher);
 private:
  std::mutex mutex_;
  std::thread worker_;
  std::unique_ptr<NativeSession> pending_;
  std::unique_ptr<NativeSession> session_;
  NativeStartupState state_ = NativeStartupState::idle;
};
}
