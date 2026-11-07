// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "protected_uninstall.hpp"
#include "vapor_game_cleanup_protocol.hpp"
#include <functional>

namespace ww::steam_integration {
enum class CleanerState { complete, pending, blocked };
// The caller holds both game/state leases and a durable registry-removed
// journal throughout this call. A previously published request is observed,
// never dispatched a second time, even after restart or an unknown send result.
CleanerState RunGameCleanupHelper(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const vapor_cleaner::Request& request,
    const std::function<bool()>& journal_valid, const std::function<ProtectedState()>& observe,
    const std::function<bool()>& send_once, const std::function<void()>& wait, unsigned polls);
}  // namespace ww::steam_integration
