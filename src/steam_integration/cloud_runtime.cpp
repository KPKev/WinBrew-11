// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloud_runtime.hpp"
#include "cloud_preflight.hpp"
#include "steam/backend/cm_client.h"
#include "steam/backend/http_client.h"
#include "steam/backend/steam_cloud_runner.h"
#include "steam/local_session_repository.h"
#include "steam/session_vault.h"
#include <utility>
namespace ww::steam_integration {
class CloudRuntime::Impl {
 public:
  Impl(const std::string& root, const std::string& ca, playden::Storage& storage,
       playden::SessionVault& vault, std::shared_ptr<playden::steam::CmSharedSession> shared)
      : sessions(root, {}, &vault), cm(ca, storage), http(ca), runner(cm, http),
        service(runner, sessions), gate(service) {
    cm.SetOwner("wine11-cloud"); cm.SetSharedSession(std::move(shared));
  }
  // The gate and service stop before their runner, connection, and session repository.
  playden::LocalSessionRepository sessions;
  playden::steam::CmClient cm;
  playden::steam::HttpClient http;
  playden::steam::SteamCloudRunner runner;
  playden::steam::CloudService service;
  CloudPreflight gate;
};
CloudRuntime::CloudRuntime(const std::string& root, const std::string& ca, playden::Storage& storage,
                           playden::SessionVault& vault, std::shared_ptr<playden::steam::CmSharedSession> shared)
    : impl_(std::make_unique<Impl>(root, ca, storage, vault, std::move(shared))) {}
CloudRuntime::~CloudRuntime() = default;
CloudPreflight& CloudRuntime::preflight() { return impl_->gate; }
}
