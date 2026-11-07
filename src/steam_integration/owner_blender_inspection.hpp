// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "uninstall_journal.hpp"
#include "homebrew/runtime_host.hpp"
#include "installs/install_service.h"
namespace ww::steam_integration {
#if defined(WINE11_OWNER_CLEANUP_PROBE)
inline constexpr char kOwnerInspectionName[] = ".vapor-owner-inspect-47fc159f-096c-4399-a934-4455637be846.txt";
#else
inline constexpr char kOwnerInspectionName[] = ".vapor-owner-inspect-472aa895-6e24-4fd1-b035-9d6170ec3004.txt";
#endif
// Inspection-only build: writes one sanitized report, without resuming or
// changing a removal. Any separate durable-storage probe is uniquely named.
bool WriteOwnerBlenderInspection(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const UninstallJournal& journal,
    const playden::Storage& state_storage, const playden::homebrew::RuntimeSnapshot& host,
    const playden::installs::InstallSnapshot* installs, bool maintenance_ready);
}
