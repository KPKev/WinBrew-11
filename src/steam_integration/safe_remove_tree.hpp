// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Experimental bounded deletion primitive, not connected to uninstall.
// Name checks followed by unlinkat are not atomic against an uncooperative
// concurrent writer; the cooperative game-storage lease does not rule that
// out. A stronger exclusion contract is required before console use.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>

namespace ww::steam_integration {
struct RemoveIdentity { dev_t device; ino_t inode; uid_t owner; };
namespace safe_remove_detail {
inline bool same(const struct stat& s, const RemoveIdentity& i) {
  return s.st_dev == i.device && s.st_ino == i.inode && s.st_uid == i.owner;
}
inline bool empty_dir(int fd, dev_t device, uid_t owner, unsigned depth) {
  if (depth > 64) { errno = ELOOP; return false; }
  const int copy = dup(fd);
  if (copy < 0) return false;
  DIR* dir = fdopendir(copy);
  if (!dir) { close(copy); return false; }
  bool ok = true;
  for (;;) {
    errno = 0;
    const dirent* item = readdir(dir);
    if (!item) { if (errno) ok = false; break; }
    if (!std::strcmp(item->d_name, ".") || !std::strcmp(item->d_name, "..")) continue;
    struct stat before{};
    if (fstatat(fd, item->d_name, &before, AT_SYMLINK_NOFOLLOW) || before.st_dev != device || before.st_uid != owner) { ok = false; break; }
    if (S_ISDIR(before.st_mode)) {
      int child = openat(fd, item->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      struct stat opened{}, current{};
      if (child < 0 || fstat(child, &opened) || opened.st_dev != before.st_dev || opened.st_ino != before.st_ino ||
          !empty_dir(child, device, owner, depth + 1) ||
          fstatat(fd, item->d_name, &current, AT_SYMLINK_NOFOLLOW) || current.st_dev != opened.st_dev ||
          current.st_ino != opened.st_ino || fsync(child) || unlinkat(fd, item->d_name, AT_REMOVEDIR) || fsync(fd)) {
        if (child >= 0) close(child);
        ok = false; break;
      }
      close(child);
    } else if (S_ISREG(before.st_mode) || S_ISLNK(before.st_mode)) {
      // unlinkat removes a symlink itself and never follows its target.
      struct stat current{};
      if (fstatat(fd, item->d_name, &current, AT_SYMLINK_NOFOLLOW) || current.st_dev != before.st_dev ||
          current.st_ino != before.st_ino || unlinkat(fd, item->d_name, 0) || fsync(fd)) { ok = false; break; }
    } else { errno = EINVAL; ok = false; break; }
  }
  closedir(dir);
  return ok;
}
}  // namespace safe_remove_detail

// The caller supplies an already-open parent directory, a single child name,
// and the root identity recorded before removal was authorized. Never follows
// symlinks, crosses filesystems, or enters another owner's files.
inline bool RemoveOwnedTree(int parent, const std::string& name, const RemoveIdentity& identity) {
  if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) { errno = EINVAL; return false; }
  int root = openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (root < 0) return false;
  struct stat opened{}, current{};
  bool ok = fstat(root, &opened) == 0 && S_ISDIR(opened.st_mode) && safe_remove_detail::same(opened, identity) &&
            safe_remove_detail::empty_dir(root, identity.device, identity.owner, 0) &&
            fstatat(parent, name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 && safe_remove_detail::same(current, identity) &&
            unlinkat(parent, name.c_str(), AT_REMOVEDIR) == 0;
  close(root);
  return ok;
}
}  // namespace ww::steam_integration
