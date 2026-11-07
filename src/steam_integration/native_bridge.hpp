// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native_startup.hpp"
namespace ww { struct Shell; }
namespace ww::steam_integration {
// Keep this object alive through every shell frame, and destroy it before Shell.
class NativeBridge {
 public:
  explicit NativeBridge(ww::Shell& shell);
  ~NativeBridge();
  NativeBridge(const NativeBridge&) = delete;
  NativeBridge& operator=(const NativeBridge&) = delete;
  bool Start(const char* rebound_root, int anchor, const char* immutable_ca_bundle, int game_data_anchor = -1);
  NativeStartupState Tick();  // application thread, before Shell::tick()
  bool AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud);
  CloudPreflight* EnableCloud();
  bool AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver);
  std::optional<playden::homebrew::RuntimeHostConfig> GameHostConfig(
      std::string packaged_runtime_dir, CoordinatedTitleList& titles,
      playden::ps5::TitleRegistry::Transport registry, NativeTitleLauncher launcher);
  // Call on the application thread after startup succeeds. All supplied
  // callback dependencies must outlive this bridge. An empty cloud resolver
  // uses the session's private save store and the host's prepared records.
  bool StartGameHost(std::string packaged_runtime_dir, CoordinatedTitleList& titles,
                     playden::ps5::TitleRegistry::Transport registry, NativeTitleLauncher launcher,
                     CloudLocationResolver cloud_locations);
 private:
  ww::Shell& shell_;
  // Stop the host before destroying the session; retain the host object until
  // after the session's adapters are destroyed.
  std::unique_ptr<playden::homebrew::RuntimeHost> game_host_;
  NativeStartup startup_;
  ww::SteamUi* attached_ = nullptr;
};
}
