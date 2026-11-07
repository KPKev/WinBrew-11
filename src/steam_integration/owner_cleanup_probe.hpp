// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
namespace ww::steam_integration {
// Temporary owner diagnostic: uses only a uniquely named, newly created fixture.
bool RunOwnerCleanupProbe(const playden::platform::ManagedDirectory& games,
                          const playden::platform::ManagedFileLock& lease,
                          const std::filesystem::path& data_parent);
}
