// SPDX-License-Identifier: GPL-3.0-or-later
#include "protected_uninstall.hpp"
#include "uninstall_identity.hpp"
#include "safe_remove_tree.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
using playden::platform::ManagedDirectory;
using playden::platform::ManagedFileLock;
namespace {
enum class Item { missing, matching, wrong };
bool admitted(const ManagedDirectory& games, const ManagedFileLock& lease, const std::filesystem::path& parent,
              const UninstallTarget& target) {
  struct stat g{}, p{};
  return target.app && target.inode && ValidUninstallOperation(target.operation) && parent.is_absolute() &&
      games.path().filename() == "wine11-games" && games.path().parent_path() == parent &&
      games.Validate() && lease.Validate(games, "wine11-install.lock") && fstat(games.fd(), &g) == 0 &&
      lstat(parent.c_str(), &p) == 0 && S_ISDIR(p.st_mode) && p.st_uid == 0 && !(p.st_mode & 0022) &&
      g.st_dev == p.st_dev && static_cast<std::uint64_t>(g.st_dev) == target.device;
}
Item source(const ManagedDirectory& games, const UninstallTarget& t) {
  struct stat st{}; errno = 0;
  if (!games.InspectChild(std::to_string(t.app), &st)) return errno == ENOENT && games.Validate() ? Item::missing : Item::wrong;
  return S_ISDIR(st.st_mode) && st.st_uid == geteuid() && st.st_dev == t.device && st.st_ino == t.inode ? Item::matching : Item::wrong;
}
// Run only inside WithRoot. A 0733 mode is a recoverable interrupted bridge;
// seal it before inspecting anything else. Never adopt a foreign parent.
bool root_parent(const std::filesystem::path& parent, bool create) {
  const auto path = parent / ".vapor-uninstall"; struct stat st{}; errno = 0;
  if (lstat(path.c_str(), &st) != 0) {
    if (errno != ENOENT || !create || mkdir(path.c_str(), 0700) || lstat(path.c_str(), &st)) return false;
    int fd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    const bool synced = fsync(fd) == 0; const bool closed = close(fd) == 0;
    if (!synced || !closed) return false;
  }
  if (!S_ISDIR(st.st_mode) || st.st_uid != 0 || ((st.st_mode & 07777) != 0700 && (st.st_mode & 07777) != 0733)) return false;
  if ((st.st_mode & 07777) == 0733 && chmod(path.c_str(), 0700)) return false;
  return true;
}
Item destination(const std::filesystem::path& parent, const UninstallTarget& target, uid_t owner) {
  const auto path = parent / ".vapor-uninstall" / target.operation; struct stat st{}; errno = 0;
  if (lstat(path.c_str(), &st)) return errno == ENOENT ? Item::missing : Item::wrong;
  return S_ISDIR(st.st_mode) && st.st_uid == owner && st.st_dev == target.device && st.st_ino == target.inode ? Item::matching : Item::wrong;
}
bool root_sync(const std::filesystem::path& parent) {
  int fd = open((parent / ".vapor-uninstall").c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  const bool synced = fsync(fd) == 0; const bool closed = close(fd) == 0; return synced && closed;
}
bool bridge(const std::filesystem::path& parent, bool open_bridge) {
  return playden::ps5::WithRoot([&] { return root_parent(parent, false) && chmod((parent / ".vapor-uninstall").c_str(), open_bridge ? 0733 : 0700) == 0; });
}
}
ProtectedState InspectProtectedGame(const ManagedDirectory& games, const ManagedFileLock& lease,
                                    const std::filesystem::path& parent, const UninstallTarget& t) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!admitted(games, lease, parent, t)) return ProtectedState::blocked;
  const auto from = source(games, t); Item to = Item::wrong; const uid_t owner = geteuid(); bool root_missing = false;
  if (!playden::ps5::WithRoot([&] {
        struct stat st{}; errno = 0;
        if (lstat((parent / ".vapor-uninstall").c_str(), &st) != 0) {
          root_missing = errno == ENOENT; return root_missing;
        }
        if (!root_parent(parent, false)) return false;
        to = destination(parent, t, owner); return root_sync(parent);
      }))
    return ProtectedState::blocked;
  if (!games.Sync() || !admitted(games, lease, parent, t)) return ProtectedState::blocked;
  if (root_missing) return from == Item::matching ? ProtectedState::original : ProtectedState::blocked;
  if (from == Item::matching && to == Item::missing) return ProtectedState::original;
  if (from == Item::missing && to == Item::matching) return ProtectedState::retired;
  if (from == Item::missing && to == Item::missing) return ProtectedState::removed;
  return ProtectedState::blocked;
}
ProtectedState RetireProtectedGame(const ManagedDirectory& games, const ManagedFileLock& lease,
                                   const std::filesystem::path& parent, const UninstallTarget& t) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!admitted(games, lease, parent, t) || source(games, t) != Item::matching) return ProtectedState::blocked;
  Item to = Item::wrong; const uid_t owner = geteuid();
  if (!playden::ps5::WithRoot([&] { if (!root_parent(parent, true)) return false; to = destination(parent, t, owner); return true; }) ||
      to != Item::missing || !admitted(games, lease, parent, t) || !bridge(parent, true)) return ProtectedState::blocked;
  const auto src = games.path() / std::to_string(t.app), dst = parent / ".vapor-uninstall" / t.operation;
  const bool renamed = rename(src.c_str(), dst.c_str()) == 0;
  const bool sealed = bridge(parent, false); // Attempt to seal even when rename failed.
  if (!renamed || !sealed) return ProtectedState::blocked;
  return InspectProtectedGame(games, lease, parent, t) == ProtectedState::retired ? ProtectedState::retired : ProtectedState::blocked;
}
ProtectedState RemoveProtectedGame(const ManagedDirectory& games, const ManagedFileLock& lease,
                                   const std::filesystem::path& parent, const UninstallTarget& t) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const uid_t owner = geteuid();
  if (InspectProtectedGame(games, lease, parent, t) != ProtectedState::retired || !bridge(parent, true)) return ProtectedState::blocked;
  const auto path = parent / ".vapor-uninstall" / t.operation;
  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); struct stat st{};
  const bool opened = fd >= 0 && fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == geteuid() &&
      st.st_dev == t.device && st.st_ino == t.inode;
  const bool sealed = bridge(parent, false);
  if (!opened || !sealed) { if (fd >= 0) close(fd); return ProtectedState::blocked; }
  if (!safe_remove_detail::empty_dir(fd, st.st_dev, st.st_uid, 0) || fsync(fd)) { close(fd); return ProtectedState::blocked; }
  close(fd);
  if (!playden::ps5::WithRoot([&] {
        if (!root_parent(parent, false) || destination(parent, t, owner) != Item::matching || rmdir(path.c_str())) return false;
        return root_sync(parent);
      })) return ProtectedState::blocked;
  return InspectProtectedGame(games, lease, parent, t) == ProtectedState::removed ? ProtectedState::removed : ProtectedState::blocked;
}
}
