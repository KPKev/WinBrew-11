// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "uninstall_quarantine.hpp"
#include <filesystem>
namespace ww::steam_integration {
enum class ProtectedState { original, retired, removed, blocked };
// Single-worker operations, under a durable uninstall intent and both leases.
// Only the supplied operation UUID and recorded device/inode are admitted.
// The private root-owned parent prevents new path opens after it is sealed.
// Stopped-title and absence of previously opened foreign descriptors are the
// caller's responsibility; this is not protection against a privileged writer.
ProtectedState InspectProtectedGame(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const std::filesystem::path& parent, const UninstallTarget& target);
ProtectedState RetireProtectedGame(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const std::filesystem::path& parent, const UninstallTarget& target);
ProtectedState RemoveProtectedGame(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const std::filesystem::path& parent, const UninstallTarget& target);
}
