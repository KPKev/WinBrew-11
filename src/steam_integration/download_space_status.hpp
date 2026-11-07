// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "steam/backend/depot_installer.h"
#include <string>
namespace ww::steam_integration {
inline constexpr const char* kSpaceQueryUnavailable = "wine11-free-space-unavailable";
inline constexpr const char* kSpaceQueryMessage = "Free space is unavailable. Check the storage and try again.";
inline bool IsSpaceQueryUnavailable(playden::steam::InstallError error, const std::string& detail) {
  return error == playden::steam::InstallError::kStorage && detail == kSpaceQueryUnavailable;
}
}
