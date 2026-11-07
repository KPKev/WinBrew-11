// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ready_launch.hpp"
#include "steam/cloud_service.h"
namespace ww::steam_integration {
// Application-thread adapter for a single Playden Cloud sync. The caller must
// supply paths from the verified current install and prepared runtime record,
// and this must be the service's sole sync requester. Keep polling while Busy()
// even after an account change, so the in-flight operation is observed to finish.
class CloudPreflight {
 public:
  explicit CloudPreflight(playden::steam::CloudService& service) : service_(service) {}
  bool Begin(playden::steam::CloudLocation location, std::uint64_t account, std::uint64_t generation);
  // Resolve only the current completed conflict or account-confirmation
  // report. The user must make this choice explicitly in the interface.
  bool Resolve(std::uint32_t app, std::uint64_t account, std::uint64_t generation,
               playden::steam::CloudResolution choice, bool confirm_account);
  CloudGateResult Poll(std::uint32_t app, std::uint64_t account, std::uint64_t generation);
  void Observe(std::uint64_t account, std::uint64_t generation);
  bool Busy() const { return active_; }
  std::uint32_t ChoiceApp(std::uint64_t account, std::uint64_t generation) const;
  std::uint32_t SkipApp(std::uint64_t account, std::uint64_t generation) const;
  bool AllowWithoutCloud(std::uint32_t app, std::uint64_t account, std::uint64_t generation);
  const std::string& ChoiceGameRoot() const { return location_.game_root; }
 private:
  playden::steam::CloudService& service_;
  playden::GameId game_;
  playden::steam::CloudLocation location_;
  playden::steam::CloudState last_state_ = playden::steam::CloudState::kIdle;
  std::uint64_t completed_serial_ = 0;
  std::uint64_t account_ = 0, generation_ = 0, serial_ = 0;
  bool active_ = false, invalidated_ = false;
  CloudGateResult result_;
};
}
