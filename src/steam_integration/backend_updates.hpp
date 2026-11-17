// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../steam_ui.hpp"
#include "update/update_service.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace ww::steam_integration {
inline constexpr const char* BackendManifestAsset = "vapor-backend-update.json";
inline constexpr unsigned BackendFrontendAbi = 1;
// A stock Playden archive is not a Vapor update. The additional contract names
// the complete app/runtime integration qualified by the Vapor publisher.
bool ValidateBackendManifest(std::string_view json, std::string* error);
std::optional<playden::update::Version> InstalledVaporVersion(const std::string& app_root);
class BackendUpdates : public BackendUpdateProvider, public std::enable_shared_from_this<BackendUpdates> {
 public:
  BackendUpdates(playden::update::UpdateConfig config, std::string installed_backend);
  ~BackendUpdates();
  void Start();
  bool Check();
  bool Install();
  BackendUpdateStatus Snapshot() const;
  std::function<bool()> RestartAction(bool rollback);
  void Shutdown();
 private:
  playden::update::Fetch fetch_;
  std::unique_ptr<playden::update::UpdateService> update_;
  mutable std::mutex mutex_;
  BackendUpdateStatus status_;
  std::thread checker_;
  std::atomic<bool> cancel_{false}, checking_{false};
  std::string release_url_;
};
}
