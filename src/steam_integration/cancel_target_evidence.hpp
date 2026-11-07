// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cstdint>

namespace ww::steam_integration {
// Experimental evidence only. Neither method permits removal or registry edits.
// Call without the install-service mutex; these methods acquire the UID guard.
struct CancelTargetEvidence {
  std::uint32_t app = 0;
  std::uint64_t nonce = 0, device = 0, inode = 0;
};
bool InspectCancelTarget(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, std::uint64_t nonce, CancelTargetEvidence& result);
bool CapturePreparedCancelTarget(const playden::platform::ManagedDirectory& games,
                                 const playden::platform::ManagedFileLock& lease,
                                 const CancelTargetEvidence& prepared);
bool CaptureCancelTarget(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, std::uint64_t nonce, CancelTargetEvidence& result);
bool VerifyCancelTarget(const playden::platform::ManagedDirectory& games,
                        const playden::platform::ManagedFileLock& lease,
                        const CancelTargetEvidence& evidence);
}
