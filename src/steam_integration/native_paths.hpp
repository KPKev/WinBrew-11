// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <optional>

namespace ww::steam_integration {
// Owns the state writer lease until the runtime has stopped. The game directory
// is transferred to Runtime::EnableDownloads, which takes its own writer lease.
struct NativePaths {
  playden::platform::ManagedDirectory state;
  playden::platform::ManagedFileLock state_lease;
  playden::platform::ManagedDirectory games;
};

// Call on a startup worker after platform elevation has completed. anchored_fd
// must name the Wine 11 download directory opened before any root change.
// This function never touches Playden's own state or game directories.
// A nonnegative data anchor selects the dedicated main-drive game folder;
// the optional path parameter is for an isolated filesystem test only.
std::optional<NativePaths> PrepareNativePaths(const char* rebound_root, int anchored_fd,
    int data_anchor = -1, const char* data_path = "/data");
// Nothing means the session path or lease is unsafe; false means no saved
// session. This never reads or prints credential bytes.
std::optional<bool> HasSavedSteamSession(const NativePaths& paths);
}
