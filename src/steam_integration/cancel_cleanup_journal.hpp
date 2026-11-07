// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "platform/storage.h"
#include "cancel_target_evidence.hpp"
#include "cancel_winner_evidence.hpp"
#include <cstdint>
#include <mutex>

namespace ww::steam_integration {
// Experimental single-operation durable journal. Call only without the install
// service mutex held; each operation may wait for the process-wide UID lock.
// References must outlive this object. No operation here authorizes deletion.
class CancelCleanupJournal {
 public:
  enum class State { absent, prepared, intent, winner, completed, blocked };
  struct Entry { State state = State::blocked; std::uint32_t app = 0; std::uint64_t operation = 0; CancelTargetEvidence target{}; CancelWinnerEvidence winner{}; };
  CancelCleanupJournal(const playden::platform::ManagedDirectory& state,
      const playden::platform::ManagedFileLock& state_lease,
      const playden::platform::ManagedDirectory& games,
      const playden::platform::ManagedFileLock& games_lease);
  Entry Read();
  bool Prepare(std::uint32_t app, std::uint64_t operation, const CancelTargetEvidence& target);
  bool CaptureMarker(std::uint32_t app, std::uint64_t operation);
  bool BindMarker(std::uint32_t app, std::uint64_t operation);
  bool RecordWinner(std::uint32_t app, std::uint64_t operation, const CancelWinnerEvidence& winner);
  bool Complete(std::uint32_t app, std::uint64_t operation);
 private:
  Entry ReadLocked();
  bool Identity(std::uint64_t& device, std::uint64_t& inode) const;
  const playden::platform::ManagedDirectory& state_;
  const playden::platform::ManagedFileLock& state_lease_;
  const playden::platform::ManagedDirectory& games_;
  const playden::platform::ManagedFileLock& games_lease_;
  playden::Storage storage_;
  std::mutex mutex_;
};
}
