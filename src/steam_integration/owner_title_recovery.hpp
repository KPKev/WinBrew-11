// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "uninstall_journal.hpp"
#include <filesystem>
#include <string>
namespace ww::steam_integration {
struct OwnerTitleRecoveryTarget {
  std::filesystem::path data_parent, homebrew_parent;
  std::string quarantine_name, title, receipt_name, receipt_text;
  std::uint32_t app = 0;
  dev_t device{}; ino_t inode{};
};
OwnerTitleRecoveryTarget BlenderOwnerTitleRecovery();
// Claim only after the exact failed retire has a verified, sealed backup.
bool ClaimOwnerTitleRecovery(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const UninstallJournal::Entry& entry,
    const OwnerTitleRecoveryTarget& target, std::int64_t now);
// Call only after the native title is confirmed absent and the install worker is stopped.
bool CleanupOwnerTitleQuarantine(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const OwnerTitleRecoveryTarget& target);
bool OwnerTitleQuarantineAbsent(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const OwnerTitleRecoveryTarget& target);
}
