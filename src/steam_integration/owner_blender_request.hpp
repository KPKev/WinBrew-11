// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cstdint>
namespace ww::steam_integration {
// This request exists only in an explicitly enabled, temporary owner build.
inline constexpr char kOwnerBlenderRequestName[] = ".vapor-owner-blender-e5fd956c-1214-4732-a352-0765f2a15552.request";
inline constexpr char kOwnerBlenderRequestText[] = "remove 365670 FAKE78008 e5fd956c-1214-4732-a352-0765f2a15552\n";
enum class OwnerBlenderClaim { absent, rejected, claimed };
OwnerBlenderClaim ClaimOwnerBlenderRequest(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, std::int64_t now);
}
