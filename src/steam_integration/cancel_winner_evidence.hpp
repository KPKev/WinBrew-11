// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cstdint>
#include <string>
namespace ww::steam_integration {
// Read-only experimental evidence; it neither records a winner nor changes a registry.
struct CancelWinnerEvidence {
  std::uint32_t app = 0;
  std::uint64_t directory_device = 0, directory_inode = 0, record_device = 0, record_inode = 0;
  std::string record;
};
bool CaptureCancelWinner(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, CancelWinnerEvidence& result);
bool VerifyCancelWinner(const playden::platform::ManagedDirectory& games,
                        const playden::platform::ManagedFileLock& lease,
                        const CancelWinnerEvidence& evidence);
}
