// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_bridge.hpp"
#include "../shell.hpp"
#include <utility>
namespace ww::steam_integration {
NativeBridge::NativeBridge(ww::Shell& shell) : shell_(shell) {}
NativeBridge::~NativeBridge() {
  if (game_host_) game_host_->Shutdown();
  if (shell_.steam == attached_) shell_.steam = nullptr;
}
bool NativeBridge::Start(const char* root, int anchor, const char* ca, int game_data_anchor) {
  if (shell_.steam) return false;
  return startup_.Start(root, anchor, ca, game_data_anchor);
}
NativeStartupState NativeBridge::Tick() {
  const auto state = startup_.Poll();
  if (state == NativeStartupState::ready && !attached_ && !shell_.steam) {
    attached_ = startup_.ui();
    shell_.steam = attached_;
  }
  return state;
}
bool NativeBridge::AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud) {
  return startup_.AttachGameRuntime(host, std::move(cloud));
}
CloudPreflight* NativeBridge::EnableCloud() { return startup_.EnableCloud(); }
bool NativeBridge::AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver) {
  return startup_.AttachGameRuntimeWithCloud(host, std::move(resolver));
}
std::optional<playden::homebrew::RuntimeHostConfig> NativeBridge::GameHostConfig(
    std::string dir, CoordinatedTitleList& titles, playden::ps5::TitleRegistry::Transport registry,
    NativeTitleLauncher launcher) {
  return startup_.GameHostConfig(std::move(dir), titles, std::move(registry), std::move(launcher));
}
bool NativeBridge::StartGameHost(std::string dir, CoordinatedTitleList& titles,
                                 playden::ps5::TitleRegistry::Transport registry, NativeTitleLauncher launcher,
                                 CloudLocationResolver locations) {
  if (game_host_) return false;
  auto config = startup_.GameHostConfig(std::move(dir), titles, std::move(registry), std::move(launcher));
  if (!config) return false;
  auto host = std::make_unique<playden::homebrew::RuntimeHost>(std::move(*config));
  if (!host->Start()) return false;
  if (!locations) locations = startup_.CloudLocations(*host);
  if (!locations) { host->Shutdown(); return false; }
  if (!startup_.AttachGameRuntimeWithCloud(*host, std::move(locations))) {
    host->Shutdown(); return false;
  }
  game_host_ = std::move(host);
  return true;
}
}
