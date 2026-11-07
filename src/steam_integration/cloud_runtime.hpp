// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <string>
namespace playden { class Storage; class SessionVault; }
namespace playden::steam { class CmSharedSession; }
namespace ww::steam_integration {
class CloudPreflight;
// Composes Playden's real Steam Cloud worker using Wine 11's account vault and
// shared connection. Nothing syncs until a caller starts a preflight.
class CloudRuntime {
 public:
  CloudRuntime(const std::string& state_root, const std::string& ca_bundle,
               playden::Storage& storage, playden::SessionVault& vault,
               std::shared_ptr<playden::steam::CmSharedSession> shared);
  ~CloudRuntime();
  CloudRuntime(const CloudRuntime&) = delete;
  CloudRuntime& operator=(const CloudRuntime&) = delete;
  CloudPreflight& preflight();
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}
