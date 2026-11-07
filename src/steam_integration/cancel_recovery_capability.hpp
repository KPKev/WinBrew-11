// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"

namespace ww::steam_integration {
// NativeSession owns these objects and outlives its Runtime and service worker.
// This only checks the existing state capability; it is not a journal.
class CancelRecoveryCapability {
 public:
  CancelRecoveryCapability(const playden::platform::ManagedDirectory& state,
                           const playden::platform::ManagedFileLock& lease)
      : state_(state), lease_(lease) {}
  bool Valid() const {
    return state_.Validate() && lease_.Validate(state_, "wine11-state.lock");
  }
  bool BeforeScan() const {
    if (!Valid()) return false;
    // Recovery and durable tombstone reconciliation are not implemented.
    // Fail closed until they are; a valid lease is not recovery evidence.
    return false;
  }
 private:
  const playden::platform::ManagedDirectory& state_;
  const playden::platform::ManagedFileLock& lease_;
};
}
