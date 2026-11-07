// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "platform/storage.h"
#include <cstdint>
#include <string>

namespace ww::steam_integration {
// Persistence only: this journal never removes a file, title or registry row.
// A single serialized worker must hold both leases and independently verify
// each completed effect before advancing. Unknown/corrupt state must block
// new game installs; this journal alone does not implement the startup gate.
class UninstallJournal {
 public:
  enum class Phase : unsigned { absent = 0, intent = 1, title_removed = 2,
                                game_retired = 3, registry_removed = 4, quarantine_removed = 5,
                                queue_intent = 6, download_queued = 7, blocked = 8 };
  struct Entry {
    Phase phase = Phase::blocked;
    std::uint32_t app = 0;
    std::string operation;
    std::uint64_t root_device = 0, root_inode = 0;
    std::string title;
    bool reinstall = false;
    std::uint64_t account = 0;  // Stable Steam account ID; never an authentication secret.
    std::string registry_before{}; // Private, exact snapshot; no credentials.
  };
  UninstallJournal(const playden::platform::ManagedDirectory& state,
                   const playden::platform::ManagedFileLock& state_lock,
                   const playden::platform::ManagedDirectory& games,
                   const playden::platform::ManagedFileLock& games_lock);
  Entry Read() const;
  // The operation is persisted before the caller performs any removal.
  bool Begin(const Entry& entry);
  bool Advance(const Entry& expected, Phase next);
  bool Clear(const Entry& expected);
 private:
  bool ParentIdentity(std::uint64_t& device, std::uint64_t& inode) const;
  const playden::platform::ManagedDirectory& state_;
  const playden::platform::ManagedFileLock& state_lock_;
  const playden::platform::ManagedDirectory& games_;
  const playden::platform::ManagedFileLock& games_lock_;
  playden::Storage storage_;
};
}  // namespace ww::steam_integration

