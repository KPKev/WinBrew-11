// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include "steam/app_details_service.h"
#include <cstdint>
#include <string>
namespace ww::steam_integration {
enum class LaunchDetailsState { idle, waiting, ready, failed };
// Application-thread adapter for Playden's asynchronous Steam app details.
class LaunchDetails {
 public:
  explicit LaunchDetails(playden::steam::AppDetailsService& service) : service_(service) {}
  bool Request(playden::homebrew::RuntimePrepareRequest request, std::uint64_t account, std::uint64_t generation);
  void Tick(std::uint64_t account, std::uint64_t generation);
  void Cancel();
  LaunchDetailsState state() const { return state_; }
  const playden::homebrew::RuntimePrepareRequest& request() const { return request_; }
  const std::string& message() const { return message_; }
 private:
  playden::steam::AppDetailsService& service_;
  playden::homebrew::RuntimePrepareRequest request_;
  std::uint64_t account_ = 0, generation_ = 0;
  LaunchDetailsState state_ = LaunchDetailsState::idle;
  std::string message_;
};
}
