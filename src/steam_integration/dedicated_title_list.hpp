// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "title_list_writer.hpp"
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <memory>
#include <mutex>
namespace ww::steam_integration {
// A Wine 11-only title-rule directory and a cooperative exclusive lease.
// On the console the caller must admit /data/wine11/title-rule as the root-owned
// private directory and pre-create wine11-title.lock there. The directory and
// its ancestors must remain stable; no uncoordinated root writer is permitted.
class DedicatedTitleList final : public CoordinatedTitleList {
 public:
  static std::unique_ptr<DedicatedTitleList> Open(playden::platform::ManagedDirectory&& directory);
  std::optional<std::string> Read() override;
  TitleListReplace ReplaceIfUnchanged(const std::string& expected, const std::string& replacement) override;
 private:
  DedicatedTitleList(playden::platform::ManagedDirectory&& directory, playden::platform::ManagedFileLock&& lease);
  bool Valid() const;
  playden::platform::ManagedDirectory directory_;
  playden::platform::ManagedFileLock lease_;
  std::recursive_mutex mutex_;
};
// Run on a startup worker after elevation. `data` must be the independently
// anchored console /data directory (a temporary test namespace is allowed).
// Existing folders and lock files are never chmodded or replaced.
std::unique_ptr<DedicatedTitleList> ProvisionDedicatedTitleList(const playden::platform::ManagedDirectory& data, int anchored_fd);
}
