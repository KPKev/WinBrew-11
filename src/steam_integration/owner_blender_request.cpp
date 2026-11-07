// SPDX-License-Identifier: GPL-3.0-or-later
#include "owner_blender_request.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
OwnerBlenderClaim ClaimOwnerBlenderRequest(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, std::int64_t now) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto valid = [&] { return games.policy() == playden::platform::DirectoryPolicy::kPrivate &&
      games.Validate() && lease.Validate(games, "wine11-install.lock"); };
  if (!valid() || now <= 0) return OwnerBlenderClaim::rejected;
  struct stat named{}; errno = 0;
  if (!games.InspectChild(kOwnerBlenderRequestName, &named))
    return errno == ENOENT && valid() ? OwnerBlenderClaim::absent : OwnerBlenderClaim::rejected;
  const int fd = games.OpenRegularFile(kOwnerBlenderRequestName, true, 0600);
  if (fd < 0) return OwnerBlenderClaim::rejected;
  struct stat before{}, after{}, current{};
  constexpr std::size_t length = sizeof(kOwnerBlenderRequestText) - 1;
  char text[length]{};
  bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 &&
      before.st_uid == geteuid() && before.st_size == static_cast<off_t>(length) &&
      before.st_dev == named.st_dev && before.st_ino == named.st_ino &&
      before.st_mtime <= now && before.st_mtime >= now - 600;
  std::size_t used = 0;
  while (ok && used < length) {
    const auto n = read(fd, text + used, length - used);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    used += static_cast<std::size_t>(n);
  }
  ok = ok && std::memcmp(text, kOwnerBlenderRequestText, length) == 0 && fsync(fd) == 0 &&
      fstat(fd, &after) == 0 && after.st_dev == before.st_dev && after.st_ino == before.st_ino &&
      after.st_mode == before.st_mode && after.st_uid == before.st_uid && after.st_nlink == 1 &&
      after.st_size == before.st_size && after.st_mtim.tv_sec == before.st_mtim.tv_sec &&
      after.st_mtim.tv_nsec == before.st_mtim.tv_nsec && after.st_ctim.tv_sec == before.st_ctim.tv_sec &&
      after.st_ctim.tv_nsec == before.st_ctim.tv_nsec;
  const bool closed = close(fd) == 0;
  if (!ok || !closed || !valid() || !games.InspectChild(kOwnerBlenderRequestName, &current) ||
      current.st_dev != before.st_dev || current.st_ino != before.st_ino || current.st_mode != before.st_mode ||
      current.st_uid != before.st_uid || current.st_nlink != 1 || current.st_size != before.st_size ||
      current.st_mtim.tv_sec != before.st_mtim.tv_sec || current.st_mtim.tv_nsec != before.st_mtim.tv_nsec ||
      current.st_ctim.tv_sec != before.st_ctim.tv_sec || current.st_ctim.tv_nsec != before.st_ctim.tv_nsec ||
      !valid()) return OwnerBlenderClaim::rejected;
  if (!games.RemoveRegularFile(kOwnerBlenderRequestName) || !games.Sync() || !valid()) return OwnerBlenderClaim::rejected;
  return OwnerBlenderClaim::claimed;
}
}
