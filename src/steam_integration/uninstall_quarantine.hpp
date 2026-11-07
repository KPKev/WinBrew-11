// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cstdint>
#include <string>
namespace ww::steam_integration {
struct UninstallTarget {
  std::uint32_t app = 0;
  std::string operation; // UUID; destination is derived, never an arbitrary path.
  std::uint64_t device = 0, inode = 0;
};
enum class QuarantineState { original, moved, blocked };
// Moves only the validated, installed game folder into a private quarantine.
// Never deletes game contents. Call from the serialized worker without the
// install-service mutex; the caller must hold an authenticated durable intent.
QuarantineState InspectQuarantine(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const UninstallTarget& target);
QuarantineState QuarantineInstalled(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const UninstallTarget& target);
}
