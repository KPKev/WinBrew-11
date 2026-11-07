// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_startup.hpp"
#include "native_session.hpp"
#include "native_trust.hpp"
#include <unistd.h>
#include <utility>

namespace ww::steam_integration {
NativeStartup::NativeStartup() = default;
NativeStartup::~NativeStartup() {
  if (worker_.joinable()) worker_.join();
}
bool NativeStartup::Start(const char* root, int anchor, const char* ca, int game_data_anchor) {
  if (!root || !*root || !ca || !*ca || anchor < 0) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != NativeStartupState::idle) return false;
  const int owned_anchor = dup(anchor);
  if (owned_anchor < 0) { state_ = NativeStartupState::failed; return false; }
  const int owned_data = game_data_anchor >= 0 ? dup(game_data_anchor) : game_data_anchor;
  if (game_data_anchor >= 0 && owned_data < 0) { close(owned_anchor); state_ = NativeStartupState::failed; return false; }
  try {
    const std::string root_copy(root), ca_copy(ca);
    state_ = NativeStartupState::starting;
    worker_ = std::thread([this, owned_anchor, owned_data, root_copy, ca_copy] {
      std::unique_ptr<NativeSession> created;
      try {
        if (VerifyPackagedSteamCa(ca_copy.c_str()))
          created = NativeSession::Create(root_copy.c_str(), owned_anchor, ca_copy.c_str(), owned_data);
      }
      catch (...) { }
      close(owned_anchor);
      if (owned_data >= 0) close(owned_data);
      std::lock_guard<std::mutex> lock(mutex_);
      pending_ = std::move(created);
      state_ = pending_ ? NativeStartupState::ready : NativeStartupState::failed;
    });
  } catch (...) {
    close(owned_anchor);
    if (owned_data >= 0) close(owned_data);
    state_ = NativeStartupState::failed;
    return false;
  }
  return true;
}
NativeStartupState NativeStartup::Poll() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == NativeStartupState::ready && pending_) session_ = std::move(pending_);
  return state_;
}
ww::SteamUi* NativeStartup::ui() {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ ? &session_->ui() : nullptr;
}
bool NativeStartup::AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud) {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ && session_->AttachGameRuntime(host, std::move(cloud));
}
CloudPreflight* NativeStartup::EnableCloud() {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ ? session_->EnableCloud() : nullptr;
}
bool NativeStartup::AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver) {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ && session_->AttachGameRuntimeWithCloud(host, std::move(resolver));
}
CloudLocationResolver NativeStartup::CloudLocations(playden::homebrew::RuntimeHost& host) {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ ? session_->CloudLocations(host) : CloudLocationResolver{};
}
std::optional<playden::homebrew::RuntimeHostConfig> NativeStartup::GameHostConfig(
    std::string dir, CoordinatedTitleList& titles, playden::ps5::TitleRegistry::Transport registry,
    NativeTitleLauncher launcher) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!session_) return std::nullopt;
  return session_->GameHostConfig(std::move(dir), titles, std::move(registry), std::move(launcher));
}
}
