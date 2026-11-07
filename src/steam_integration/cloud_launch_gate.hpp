// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cloud_preflight.hpp"
#include <functional>
#include <optional>
namespace ww::steam_integration {
// The resolver returns paths from the admitted runtime record and private save
// store. Its state and CloudPreflight must outlive this gate and ReadyLaunch.
using CloudLocationResolver = std::function<std::optional<playden::steam::CloudLocation>(std::uint32_t)>;
class CloudLaunchGate {
 public:
  CloudLaunchGate(CloudPreflight& preflight, CloudLocationResolver resolver);
  bool Begin(std::uint32_t app, const std::string& installed_root, std::uint64_t account,
             std::uint64_t generation, std::string* error);
  CloudGateResult Poll(std::uint32_t app, std::uint64_t account, std::uint64_t generation);
 private:
  CloudPreflight& preflight_;
  CloudLocationResolver resolver_;
};
}
