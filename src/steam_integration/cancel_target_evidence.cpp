// SPDX-License-Identifier: GPL-3.0-or-later
#include "cancel_target_evidence.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
constexpr const char* kMarker = ".wine11-cancel-intent";
bool Admitted(const playden::platform::ManagedDirectory& games, const playden::platform::ManagedFileLock& lease) {
  return games.Validate() && lease.Validate(games, "wine11-install.lock");
}
int OpenTarget(const playden::platform::ManagedDirectory& games, std::uint32_t app) {
  if (!app) return -1;
  return openat(games.fd(), std::to_string(app).c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}
bool ExistingPlaydenMarker(int target) {
  int fd = openat(target, ".playden-download", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd >= 0) { struct stat s{}; const bool ok = fstat(fd, &s) == 0 && S_ISDIR(s.st_mode); return close(fd) == 0 && ok; }
  fd = openat(target, ".playden-install.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) return false;
  struct stat s{}; const bool ok = fstat(fd, &s) == 0 && S_ISREG(s.st_mode);
  return close(fd) == 0 && ok;
}
std::string Text(std::uint32_t app, std::uint64_t nonce) {
  return "wine11-cancel-v1 " + std::to_string(app) + " " + std::to_string(nonce) + "\n";
}
bool SameNamedTarget(const playden::platform::ManagedDirectory& games, std::uint32_t app, const struct stat& expected) {
  int fd = OpenTarget(games, app);
  if (fd < 0) return false;
  struct stat current{};
  const bool same = fstat(fd, &current) == 0 && S_ISDIR(current.st_mode) &&
      current.st_dev == expected.st_dev && current.st_ino == expected.st_ino;
  return close(fd) == 0 && same;
}
}
bool InspectCancelTarget(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, std::uint64_t nonce, CancelTargetEvidence& result) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!nonce || !Admitted(games, lease)) return false;
  const int target = OpenTarget(games, app);
  if (target < 0) return false;
  struct stat info{};
  const bool ok = fstat(target, &info) == 0 && S_ISDIR(info.st_mode) && ExistingPlaydenMarker(target) &&
      SameNamedTarget(games, app, info) && Admitted(games, lease);
  const bool closed = close(target) == 0;
  if (!ok || !closed) return false;
  result = {app, nonce, static_cast<std::uint64_t>(info.st_dev), static_cast<std::uint64_t>(info.st_ino)};
  return true;
}
static bool Capture(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, std::uint64_t nonce, const CancelTargetEvidence* expected, CancelTargetEvidence& result) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!nonce || !Admitted(games, lease)) return false;
  const int target = OpenTarget(games, app);
  if (target < 0) return false;
  struct stat info{};
  bool ok = fstat(target, &info) == 0 && S_ISDIR(info.st_mode) && ExistingPlaydenMarker(target) &&
      SameNamedTarget(games, app, info) && Admitted(games, lease);
  if (ok && expected) ok = expected->app == app && expected->nonce == nonce &&
      expected->device == static_cast<std::uint64_t>(info.st_dev) && expected->inode == static_cast<std::uint64_t>(info.st_ino);
  if (!ok) { close(target); return false; }
  const int marker = openat(target, kMarker, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (marker < 0) { close(target); return false; }
  struct stat marker_info{};
  ok = fstat(marker, &marker_info) == 0 && S_ISREG(marker_info.st_mode) && marker_info.st_uid == geteuid() &&
      (marker_info.st_mode & 07777) == 0600 && marker_info.st_nlink == 1;
  const auto text = Text(app, nonce);
  std::size_t offset = 0;
  while (ok && offset < text.size()) {
    const auto n = write(marker, text.data() + offset, text.size() - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    offset += static_cast<std::size_t>(n);
  }
  if (ok) ok = fsync(marker) == 0;
  if (close(marker) != 0) ok = false;
  if (ok) ok = fsync(target) == 0;
  if (ok) ok = SameNamedTarget(games, app, info) && Admitted(games, lease);
  if (close(target) != 0) ok = false;
  if (!ok) return false; // A partial or published marker may remain for reconciliation.
  result = {app, nonce, static_cast<std::uint64_t>(info.st_dev), static_cast<std::uint64_t>(info.st_ino)};
  return true;
}
bool CaptureCancelTarget(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, std::uint64_t nonce, CancelTargetEvidence& result) {
  return Capture(games, lease, app, nonce, nullptr, result);
}
bool CapturePreparedCancelTarget(const playden::platform::ManagedDirectory& games,
                                 const playden::platform::ManagedFileLock& lease,
                                 const CancelTargetEvidence& prepared) {
  CancelTargetEvidence ignored;
  return Capture(games, lease, prepared.app, prepared.nonce, &prepared, ignored);
}
bool VerifyCancelTarget(const playden::platform::ManagedDirectory& games,
                        const playden::platform::ManagedFileLock& lease, const CancelTargetEvidence& evidence) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!evidence.app || !evidence.nonce || !Admitted(games, lease)) return false;
  const int target = OpenTarget(games, evidence.app);
  if (target < 0) return false;
  struct stat info{};
  bool ok = fstat(target, &info) == 0 && S_ISDIR(info.st_mode) &&
      static_cast<std::uint64_t>(info.st_dev) == evidence.device && static_cast<std::uint64_t>(info.st_ino) == evidence.inode &&
      SameNamedTarget(games, evidence.app, info);
  const int marker = ok ? openat(target, kMarker, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC) : -1;
  if (marker < 0) { close(target); return false; }
  struct stat m{}; char buffer[128];
  const auto count = read(marker, buffer, sizeof(buffer));
  ok = fstat(marker, &m) == 0 && S_ISREG(m.st_mode) && m.st_uid == geteuid() && (m.st_mode & 07777) == 0600 && m.st_nlink == 1 && count >= 0 &&
      std::string(buffer, static_cast<std::size_t>(count)) == Text(evidence.app, evidence.nonce) &&
      fsync(marker) == 0 && fsync(target) == 0;
  if (close(marker) != 0) ok = false;
  if (ok) ok = SameNamedTarget(games, evidence.app, info) && Admitted(games, lease);
  if (close(target) != 0) ok = false;
  return ok;
}
}
