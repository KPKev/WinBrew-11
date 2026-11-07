// SPDX-License-Identifier: GPL-3.0-or-later
#include "uninstall_quarantine.hpp"
#include "uninstall_identity.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
using playden::platform::DirectoryPolicy;
using playden::platform::ManagedDirectory;
using playden::platform::ManagedFileLock;
namespace {
bool Valid(const UninstallTarget& t) {
  return t.app && t.inode && ValidUninstallOperation(t.operation);
}
enum class Observation { absent, matching, other };
Observation Observe(const ManagedDirectory& dir, const std::string& name, const UninstallTarget& target) {
  struct stat st{}; errno = 0;
  if (!dir.InspectChild(name, &st)) return errno == ENOENT && dir.Validate() ? Observation::absent : Observation::other;
  if (!S_ISDIR(st.st_mode) || st.st_uid != geteuid() || st.st_dev != target.device || st.st_ino != target.inode)
    return Observation::other;
  return Observation::matching;
}
QuarantineState Inspect(const ManagedDirectory& games, const ManagedDirectory& trash, const UninstallTarget& target) {
  const auto old = Observe(games, std::to_string(target.app), target);
  const auto moved = Observe(trash, target.operation, target);
  if (old == Observation::matching && moved == Observation::absent) return QuarantineState::original;
  if (old == Observation::absent && moved == Observation::matching) return QuarantineState::moved;
  return QuarantineState::blocked;
}
}
QuarantineState InspectQuarantine(const ManagedDirectory& games, const ManagedFileLock& lease, const UninstallTarget& target) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!Valid(target) || !games.Validate() || !lease.Validate(games, "wine11-install.lock")) return QuarantineState::blocked;
  auto trash = games.OpenChild(".vapor-retired", DirectoryPolicy::kPrivate, false);
  if (!trash) return QuarantineState::blocked;
  const auto state = Inspect(games, *trash, target);
  // A prior rename can have succeeded before its sync failed. Re-sync both
  // parent directories before treating the move as durable.
  if (state == QuarantineState::moved && (!games.Sync() || !trash->Sync() ||
      Inspect(games, *trash, target) != QuarantineState::moved)) return QuarantineState::blocked;
  return state;
}
QuarantineState QuarantineInstalled(const ManagedDirectory& games, const ManagedFileLock& lease, const UninstallTarget& target) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!Valid(target) || !games.Validate() || !lease.Validate(games, "wine11-install.lock")) return QuarantineState::blocked;
  auto trash = games.OpenChild(".vapor-retired", DirectoryPolicy::kPrivate, true);
  if (!trash || !trash->Sync() || !games.Sync()) return QuarantineState::blocked;
  const auto before = Inspect(games, *trash, target);
  if (before == QuarantineState::moved) return games.Sync() && trash->Sync() ? before : QuarantineState::blocked;
  if (before != QuarantineState::original || !lease.Validate(games, "wine11-install.lock")) return QuarantineState::blocked;
  if (!games.RenameChild(std::to_string(target.app), *trash, target.operation, false)) return QuarantineState::blocked;
  if (!games.Sync() || !trash->Sync()) return QuarantineState::blocked;
  return Inspect(games, *trash, target) == QuarantineState::moved ? QuarantineState::moved : QuarantineState::blocked;
}
}
