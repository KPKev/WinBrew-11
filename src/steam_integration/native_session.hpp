// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include "ready_launch.hpp"
#include "cloud_launch_gate.hpp"
#include "native_host_config.hpp"
namespace ww { class SteamUi; }
namespace ww::steam_integration {
class ReadyLaunch;
class Preparation;
class CloudPreflight;
// Owns the backend and its private state lease for the entire UI lifetime.
class NativeSession {
 public:
  // On a startup worker after elevation. ca_bundle must be a verified,
  // immutable packaged file that remains readable for the session lifetime.
  // Returns null when private paths or the download lease cannot be acquired.
  static std::unique_ptr<NativeSession> Create(const char* rebound_root,
                                               int anchored_fd,
                                               const char* ca_bundle,
                                               int game_data_anchor = -1);
  ~NativeSession();
  NativeSession(const NativeSession&) = delete;
  NativeSession& operator=(const NativeSession&) = delete;
  ww::SteamUi& ui();
  // Call on the application thread; the launch adapter must outlive this session.
  bool AttachReadyLaunch(ReadyLaunch* launch);
  bool AttachPreparation(Preparation* preparation);
  // Must run on the app thread. The configured host and cloud dependencies
  // must outlive this session; false leaves the launch controls unavailable.
  bool AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud);
  bool AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver);
  CloudLocationResolver CloudLocations(playden::homebrew::RuntimeHost& host);
  CloudPreflight* EnableCloud();
  std::optional<playden::homebrew::RuntimeHostConfig> GameHostConfig(
      std::string packaged_runtime_dir, CoordinatedTitleList& titles,
      playden::ps5::TitleRegistry::Transport registry, NativeTitleLauncher launcher);
 private:
  class Impl;
  explicit NativeSession(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};
}
