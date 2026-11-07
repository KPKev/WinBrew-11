// SPDX-License-Identifier: GPL-3.0-or-later
#include "cancel_registry_reconciliation.hpp"
#include "cancel_registry_plan.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
bool NoInstallRecord(const playden::platform::ManagedDirectory& games,
                     const playden::platform::ManagedFileLock& lease, const CancelTargetEvidence& target) {
  if (!VerifyCancelTarget(games, lease, target)) return false;
  const int dir = openat(games.fd(), std::to_string(target.app).c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (dir < 0) return false;
  struct stat s{};
  bool ok = fstat(dir, &s) == 0 && static_cast<std::uint64_t>(s.st_dev) == target.device &&
      static_cast<std::uint64_t>(s.st_ino) == target.inode;
  if (ok) {
    const int record = openat(dir, ".playden-install.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (record >= 0) { close(record); ok = false; }
    else ok = errno == ENOENT;
  }
  if (close(dir) != 0) ok = false;
  return ok && VerifyCancelTarget(games, lease, target);
}
}
bool ReconcileCanceledQueueBeforeScan(CancelCleanupJournal& journal,
    const playden::platform::ManagedDirectory& games, const playden::platform::ManagedFileLock& lease) {
  // UID guard precedes journal locking and is never acquired with the service mutex.
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto intent = journal.Read();
  if (intent.state != CancelCleanupJournal::State::intent || !NoInstallRecord(games, lease, intent.target)) return false;
  playden::Storage storage(games.path().string());
  auto read = storage.ReadDurable(".playden-games", 65536);
  if (!read.ok()) return false; // Missing or uncertain registry requires explicit recovery.
  const std::string original(read.value().begin(), read.value().end());
  const auto plan = PlanCanceledQueueRemoval(original, intent.app, true);
  if (plan.state == CancelRegistryPlanState::blocked || !NoInstallRecord(games, lease, intent.target)) return false;
  if (plan.state == CancelRegistryPlanState::remove_queued) {
    const playden::Bytes bytes(plan.contents.begin(), plan.contents.end());
    if (!storage.WriteDurable(".playden-games", bytes).ok()) return false;
  }
  // A failure after rename is indeterminate; reread/sync on the next attempt.
  const auto verified = storage.ReadDurable(".playden-games", 65536);
  if (!verified.ok() || std::string(verified.value().begin(), verified.value().end()) != plan.contents ||
      !NoInstallRecord(games, lease, intent.target)) return false;
  const auto still_pending = journal.Read();
  return still_pending.state == CancelCleanupJournal::State::intent && still_pending.app == intent.app &&
      still_pending.operation == intent.operation;
}
}
