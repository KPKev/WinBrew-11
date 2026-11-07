// SPDX-License-Identifier: GPL-3.0-or-later
#include "cancel_winner_evidence.hpp"
#include "platform/ps5/title_allow_list.h"
#include "steam/backend/json.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
namespace {
bool Admitted(const playden::platform::ManagedDirectory& games, const playden::platform::ManagedFileLock& lease) {
  return games.Validate() && lease.Validate(games, "wine11-install.lock");
}
bool Same(const struct stat& left, const struct stat& right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}
int OpenDir(const playden::platform::ManagedDirectory& games, std::uint32_t app) {
  return openat(games.fd(), std::to_string(app).c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}
int OpenRecord(int dir) {
  return openat(dir, ".playden-install.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
}
bool RecordNamesApp(const std::string& bytes, std::uint32_t app) {
  auto value = playden::steam::json::Parse(bytes, {65536, 16, 4096, 65536});
  if (!value || !value->is_object()) return false;
  int apps = 0, versions = 0;
  for (const auto& [key, field] : value->object()) {
    if (key == "app_id") { ++apps; if (field.AsUint64() != app) return false; }
    if (key == "version") { ++versions; if (field.AsUint64() != 1) return false; }
  }
  return apps == 1 && versions == 1;
}
}
bool CaptureCancelWinner(const playden::platform::ManagedDirectory& games,
                         const playden::platform::ManagedFileLock& lease,
                         std::uint32_t app, CancelWinnerEvidence& result) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!app || !Admitted(games, lease)) return false;
  const int dir = OpenDir(games, app);
  if (dir < 0) return false;
  struct stat directory{}, before{}, after{};
  bool ok = fstat(dir, &directory) == 0 && S_ISDIR(directory.st_mode);
  const int record = ok ? OpenRecord(dir) : -1;
  if (record < 0) { close(dir); return false; }
  ok = fstat(record, &before) == 0 && S_ISREG(before.st_mode) && before.st_uid == geteuid() &&
      before.st_nlink == 1 && before.st_size >= 0 && before.st_size <= 65536;
  std::string bytes;
  char buffer[4096];
  while (ok) {
    const auto n = read(record, buffer, sizeof(buffer));
    if (n < 0 && errno == EINTR) continue;
    if (n < 0 || static_cast<std::size_t>(n) > 65536 - bytes.size()) { ok = false; break; }
    if (!n) break;
    bytes.append(buffer, static_cast<std::size_t>(n));
  }
  ok = ok && fstat(record, &after) == 0 && Same(before, after) && before.st_size == after.st_size &&
      bytes.size() == static_cast<std::size_t>(after.st_size) && RecordNamesApp(bytes, app);
  if (ok) ok = fsync(record) == 0 && fsync(dir) == 0;
  const int named_dir = ok ? OpenDir(games, app) : -1;
  struct stat named_directory{}, named_record{};
  const int named_file = named_dir >= 0 ? OpenRecord(named_dir) : -1;
  ok = ok && named_dir >= 0 && named_file >= 0 && fstat(named_dir, &named_directory) == 0 &&
      fstat(named_file, &named_record) == 0 && Same(directory, named_directory) && Same(after, named_record) &&
      Admitted(games, lease);
  if (named_file >= 0 && close(named_file) != 0) ok = false;
  if (named_dir >= 0 && close(named_dir) != 0) ok = false;
  if (close(record) != 0) ok = false;
  if (close(dir) != 0) ok = false;
  if (!ok) return false;
  result = {app, static_cast<std::uint64_t>(directory.st_dev), static_cast<std::uint64_t>(directory.st_ino),
            static_cast<std::uint64_t>(after.st_dev), static_cast<std::uint64_t>(after.st_ino), std::move(bytes)};
  return true;
}
bool VerifyCancelWinner(const playden::platform::ManagedDirectory& games,
                        const playden::platform::ManagedFileLock& lease, const CancelWinnerEvidence& evidence) {
  CancelWinnerEvidence observed;
  return CaptureCancelWinner(games, lease, evidence.app, observed) &&
      observed.directory_device == evidence.directory_device && observed.directory_inode == evidence.directory_inode &&
      observed.record_device == evidence.record_device && observed.record_inode == evidence.record_inode &&
      observed.record == evidence.record;
}
}
