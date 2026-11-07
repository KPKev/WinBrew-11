// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cancel_cleanup_journal.hpp"
namespace ww::steam_integration {
// Experimental pre-scan operation only. The install service and its worker
// must not exist or be running while this is called. Success does not authorize
// deletion, tombstone retirement, or unblocking service startup.
bool ReconcileCanceledQueueBeforeScan(CancelCleanupJournal& journal,
    const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease);
}
