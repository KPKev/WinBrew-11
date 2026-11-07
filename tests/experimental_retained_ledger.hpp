// Host-only prototype. This ledger does not stop workers or authorize deletion.
#pragma once
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "platform/storage.h"
#include <cstdint>
#include <map>
#include <optional>

class ExperimentalRetainedLedger {
 public:
  // Caller-supplied identities are opaque evidence, not authenticated here.
  struct Context {
    std::uint64_t account = 0, volume = 0, volume_generation = 0, attempt = 0;
    bool operator==(const Context&) const = default;
  };
  using Entries = std::map<std::uint32_t, Context>;
  ExperimentalRetainedLedger(const playden::platform::ManagedDirectory& state,
                             const playden::platform::ManagedFileLock& lease);
  // Missing, corrupt, inaccessible and unsynced are all blocked.
  std::optional<Entries> Read();
  bool Initialize();
  bool Retain(std::uint32_t app, const Context& context);
 private:
  bool Valid() const;
  std::optional<Entries> ReadLocked();
  const playden::platform::ManagedDirectory& state_;
  const playden::platform::ManagedFileLock& lease_;
  playden::Storage storage_;
};
