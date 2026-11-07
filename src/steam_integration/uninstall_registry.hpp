// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <cstdint>
#include <optional>
#include <string>
namespace ww::steam_integration {
enum class RegistryRemoval { removed, already_absent, blocked, indeterminate };
enum class ReinstallRegistry { absent, queued, installed, blocked };
enum class QueueReinstall { queued, already_queued, blocked, indeterminate };
struct RegistryRemovalPlan { std::uint32_t app = 0; std::string before, after; };
// The existing install service can create its registry with mode 0666 under
// the private games directory. Read it only while holding the games lease,
// verify its identity/owner and sync it before accepting its contents.
std::optional<std::string> ReadInstalledRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, std::size_t max_bytes = 128 * 1024);
// The plan must be persisted with the operation journal before any title or
// folder is retired. It proves exactly which other rows must remain intact.
std::optional<RegistryRemovalPlan> PlanInstalledRegistryRow(std::uint32_t app, const std::string& original);
// Call only after the install-service worker has stopped, with the games lease
// retained and the exact title and folder already retired under a durable
// uninstall intent. This method does not operate on title/game files.
RegistryRemoval RemoveInstalledRegistryRow(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan);
// Caller must already hold the durable queue-intent and the sole worker gate.
ReinstallRegistry InspectReinstallRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan);
// Call with the install worker stopped and durable queue intent recorded.
QueueReinstall QueueReinstallRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan);
}
