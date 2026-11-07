// SPDX-License-Identifier: GPL-3.0-or-later
#include "owner_title_recovery.hpp"
#include "safe_remove_tree.hpp"
#include "uninstall_identity.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
namespace {
using playden::platform::DirectoryPolicy;
using playden::platform::ManagedDirectory;
using playden::platform::ManagedFileLock;
bool Base(const ManagedDirectory& games, const ManagedFileLock& lease, const OwnerTitleRecoveryTarget& t) {
  struct stat data{}, homebrew{}, root{};
  return t.app && t.device && t.inode && !t.title.empty() && !t.quarantine_name.empty() &&
      t.quarantine_name.find('/') == std::string::npos && t.title.find('/') == std::string::npos &&
      games.policy() == DirectoryPolicy::kPrivate && games.path().parent_path() == t.data_parent &&
      games.Validate() && lease.Validate(games, "wine11-install.lock") && fstat(games.fd(), &root) == 0 &&
      lstat(t.data_parent.c_str(), &data) == 0 && S_ISDIR(data.st_mode) && data.st_uid == 0 && !(data.st_mode & 0022) &&
      lstat(t.homebrew_parent.c_str(), &homebrew) == 0 && S_ISDIR(homebrew.st_mode) && homebrew.st_uid == 0 &&
      root.st_dev == t.device && data.st_dev == t.device && homebrew.st_dev == t.device;
}
bool SourceAbsent(const OwnerTitleRecoveryTarget& t) {
  struct stat st{}; errno = 0;
  return lstat((t.homebrew_parent / t.title).c_str(), &st) != 0 && errno == ENOENT;
}
bool SyncDirectory(const std::filesystem::path& path) {
  const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = fsync(fd) == 0; const bool closed = close(fd) == 0;
  return ok && closed;
}
// Called under WithRoot: seal a bridge left open by an interrupted operation.
bool Seal(const OwnerTitleRecoveryTarget& t, bool* absent) {
  const auto root = t.data_parent / t.quarantine_name; struct stat st{}; errno = 0;
  if (lstat(root.c_str(), &st) != 0) {
    if (errno != ENOENT) return false;
    *absent = true; return SyncDirectory(t.data_parent);
  }
  *absent = false;
  if (!S_ISDIR(st.st_mode) || st.st_uid != 0 || st.st_dev != t.device ||
      ((st.st_mode & 07777) != 0700 && (st.st_mode & 07777) != 0733)) return false;
  return (st.st_mode & 07777) == 0700 || chmod(root.c_str(), 0700) == 0;
}
bool TitleMatches(const OwnerTitleRecoveryTarget& t, uid_t owner, bool* missing) {
  struct stat st{}; errno = 0;
  if (lstat((t.data_parent / t.quarantine_name / "title").c_str(), &st) != 0) {
    *missing = errno == ENOENT; return *missing;
  }
  *missing = false;
  return S_ISDIR(st.st_mode) && st.st_uid == owner && st.st_dev == t.device && st.st_ino == t.inode;
}
bool Receipt(const ManagedDirectory& games, const ManagedFileLock& lease,
             const OwnerTitleRecoveryTarget& t, std::int64_t now) {
  if (now <= 0 || t.receipt_name.empty() || t.receipt_text.empty() || t.receipt_text.size() > 256) return false;
  const int fd = games.OpenRegularFile(t.receipt_name, true, 0600);
  if (fd < 0) return false;
  struct stat before{}, after{}, named{}; std::string text(t.receipt_text.size(), '\0');
  bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 &&
      before.st_uid == geteuid() && before.st_size == static_cast<off_t>(text.size()) &&
      before.st_mtime <= now && before.st_mtime >= now - 600;
  std::size_t offset = 0;
  while (ok && offset < text.size()) {
    const auto n = read(fd, text.data() + offset, text.size() - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    offset += static_cast<std::size_t>(n);
  }
  ok = ok && text == t.receipt_text && fsync(fd) == 0 && fstat(fd, &after) == 0 &&
      before.st_dev == after.st_dev && before.st_ino == after.st_ino && before.st_uid == after.st_uid &&
      before.st_mode == after.st_mode && before.st_nlink == after.st_nlink && before.st_size == after.st_size &&
      before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
      before.st_ctim.tv_sec == after.st_ctim.tv_sec && before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
  const bool closed = close(fd) == 0;
  if (!ok || !closed || !Base(games, lease, t) || !games.InspectChild(t.receipt_name, &named) ||
      named.st_dev != before.st_dev || named.st_ino != before.st_ino || named.st_mode != before.st_mode ||
      named.st_uid != before.st_uid || named.st_nlink != 1 || named.st_size != before.st_size ||
      named.st_mtim.tv_sec != before.st_mtim.tv_sec || named.st_mtim.tv_nsec != before.st_mtim.tv_nsec ||
      named.st_ctim.tv_sec != before.st_ctim.tv_sec || named.st_ctim.tv_nsec != before.st_ctim.tv_nsec) return false;
  return games.RemoveRegularFile(t.receipt_name) && games.Sync() && Base(games, lease, t);
}
}
OwnerTitleRecoveryTarget BlenderOwnerTitleRecovery() {
  return {"/data/wine11", "/data/homebrew", ".vapor-title-FAKE78008-a962b8fc-07e1-4b48-b96e-640026f2cdde",
      "FAKE78008", ".vapor-retire-resume-a962b8fc-07e1-4b48-b96e-640026f2cdde.request",
      "resume 365670 FAKE78008 687931143 99649 a962b8fc-07e1-4b48-b96e-640026f2cdde\n",
      365670, static_cast<dev_t>(687931143ULL), static_cast<ino_t>(99649ULL)};
}
bool ClaimOwnerTitleRecovery(const ManagedDirectory& games, const ManagedFileLock& lease,
    const UninstallJournal::Entry& entry, const OwnerTitleRecoveryTarget& t, std::int64_t now) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (entry.phase != UninstallJournal::Phase::intent || entry.app != t.app || entry.title != t.title ||
      entry.reinstall || !ValidUninstallOperation(entry.operation) || !Base(games, lease, t) || !SourceAbsent(t)) return false;
  const uid_t owner = geteuid(); bool absent = false, missing = false;
  if (!playden::ps5::WithRoot([&] { return Seal(t, &absent) && !absent && TitleMatches(t, owner, &missing) && !missing; })) return false;
  if (!Receipt(games, lease, t, now)) return false;
  return Base(games, lease, t) && SourceAbsent(t) && playden::ps5::WithRoot([&] {
    return Seal(t, &absent) && !absent && TitleMatches(t, owner, &missing) && !missing;
  });
}
bool OwnerTitleQuarantineAbsent(const ManagedDirectory& games, const ManagedFileLock& lease,
                                const OwnerTitleRecoveryTarget& t) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!Base(games, lease, t) || !SourceAbsent(t)) return false;
  bool absent = false;
  return playden::ps5::WithRoot([&] { return Seal(t, &absent) && absent; }) && Base(games, lease, t);
}
bool CleanupOwnerTitleQuarantine(const ManagedDirectory& games, const ManagedFileLock& lease,
                                 const OwnerTitleRecoveryTarget& t) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!Base(games, lease, t) || !SourceAbsent(t)) return false;
  const uid_t owner = geteuid(); const auto root = t.data_parent / t.quarantine_name; const auto title = root / "title";
  bool absent = false, missing = false;
  if (!playden::ps5::WithRoot([&] {
        if (!Seal(t, &absent) || absent) return absent;
        if (!TitleMatches(t, owner, &missing)) return false;
        if (missing) return rmdir(root.c_str()) == 0 && SyncDirectory(t.data_parent);
        return chmod(root.c_str(), 0733) == 0;
      })) return false;
  if (absent || missing) return OwnerTitleQuarantineAbsent(games, lease, t);
  const int fd = open(title.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  struct stat opened{};
  const bool opened_ok = fd >= 0 && fstat(fd, &opened) == 0 && S_ISDIR(opened.st_mode) &&
      opened.st_uid == owner && opened.st_dev == t.device && opened.st_ino == t.inode;
  const bool sealed = playden::ps5::WithRoot([&] { bool gone = false; return Seal(t, &gone) && !gone; });
  if (!opened_ok || !sealed) { if (fd >= 0) close(fd); return false; }
  const bool emptied = safe_remove_detail::empty_dir(fd, t.device, owner, 0) && fsync(fd) == 0;
  const bool closed = close(fd) == 0;
  if (!emptied || !closed) return false;
  if (!playden::ps5::WithRoot([&] {
        bool gone = false, child_missing = false;
        if (!Seal(t, &gone) || gone || !TitleMatches(t, owner, &child_missing) || child_missing ||
            rmdir(title.c_str()) != 0 || !SyncDirectory(root) || rmdir(root.c_str()) != 0) return false;
        return SyncDirectory(t.data_parent);
      })) return false;
  return OwnerTitleQuarantineAbsent(games, lease, t);
}
}
