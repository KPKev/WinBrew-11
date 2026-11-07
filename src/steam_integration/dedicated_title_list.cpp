// SPDX-License-Identifier: GPL-3.0-or-later
#include "dedicated_title_list.hpp"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
namespace ww::steam_integration {
using playden::platform::DirectoryPolicy;
using playden::platform::LockMode;
using playden::platform::ManagedDirectory;
using playden::platform::ManagedFileLock;
DedicatedTitleList::DedicatedTitleList(ManagedDirectory&& directory, ManagedFileLock&& lease)
    : directory_(std::move(directory)), lease_(std::move(lease)) {}
std::unique_ptr<DedicatedTitleList> DedicatedTitleList::Open(ManagedDirectory&& directory) {
  if (directory.policy() != DirectoryPolicy::kPrivate || !directory.path().is_absolute() ||
      directory.path().filename() != "title-rule" || directory.path().parent_path().filename() != "wine11" ||
      !directory.Validate()) return {};
  auto lease = ManagedFileLock::Acquire(directory, "wine11-title.lock", LockMode::kPublisher);
  if (!lease || !lease->Validate(directory, "wine11-title.lock")) return {};
  return std::unique_ptr<DedicatedTitleList>(new DedicatedTitleList(std::move(directory), std::move(*lease)));
}
bool DedicatedTitleList::Valid() const {
  return directory_.Validate() && lease_.Validate(directory_, "wine11-title.lock");
}
std::optional<std::string> DedicatedTitleList::Read() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!Valid()) return std::nullopt;
  int fd = directory_.OpenRegularFile("allow-list", true, 0600);
  if (fd < 0) {
    if (errno == ENOENT && Valid()) return std::string{};
    return std::nullopt;
  }
  struct stat before{}, after{}, named{};
  bool ok = fstat(fd, &before) == 0 && before.st_nlink == 1 && before.st_size >= 0 && before.st_size < 65536;
  std::string bytes;
  char buffer[4096];
  while (ok) {
    ssize_t count = read(fd, buffer, sizeof(buffer));
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) { ok = false; break; }
    if (!count) break;
    bytes.append(buffer, static_cast<std::size_t>(count));
    if (bytes.size() >= 65536) ok = false;
  }
  if (fstat(fd, &after) != 0 || !directory_.InspectChild("allow-list", &named) ||
      before.st_dev != after.st_dev || before.st_ino != after.st_ino || before.st_size != after.st_size ||
      after.st_dev != named.st_dev || after.st_ino != named.st_ino || bytes.size() != static_cast<std::size_t>(before.st_size)) ok = false;
  if (close(fd) != 0 || !Valid()) ok = false;
  if (!ok) return std::nullopt;
  return bytes;
}
TitleListReplace DedicatedTitleList::ReplaceIfUnchanged(const std::string& expected, const std::string& replacement) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const auto now = Read();
  if (!now) return TitleListReplace::unavailable;
  if (*now != expected) return TitleListReplace::changed;
  if (replacement.size() >= 65536 || !Valid()) return TitleListReplace::unavailable;
  if (replacement == expected) return TitleListReplace::written;
  int fd = directory_.CreateExclusiveFile(".wine11-allow-list.new", 0600);
  if (fd < 0) return TitleListReplace::unavailable;
  bool ok = true;
  for (std::size_t offset = 0; ok && offset < replacement.size();) {
    const ssize_t count = write(fd, replacement.data() + offset, replacement.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { ok = false; break; }
    offset += static_cast<std::size_t>(count);
  }
  if (ok && fsync(fd) != 0) ok = false;
  if (close(fd) != 0) ok = false;
  if (!ok || !Valid() || !directory_.RenameChild(".wine11-allow-list.new", directory_, "allow-list", true) ||
      !directory_.Sync()) return TitleListReplace::unavailable;
  const auto written = Read();
  return written && *written == replacement ? TitleListReplace::written : TitleListReplace::unavailable;
}
std::unique_ptr<DedicatedTitleList> ProvisionDedicatedTitleList(const ManagedDirectory& data, int anchored_fd) {
  struct stat anchor{}, observed{};
  if (data.policy() != DirectoryPolicy::kApprovedTree || !data.path().is_absolute() ||
      data.path().filename() != "data" || anchored_fd < 0 || fstat(anchored_fd, &anchor) != 0 ||
      fstat(data.fd(), &observed) != 0 || !S_ISDIR(anchor.st_mode) || anchor.st_dev != observed.st_dev ||
      anchor.st_ino != observed.st_ino || !data.Validate()) return {};
  // ManagedDirectory opens every ancestor for reading, so execute-only access
  // is insufficient even though ordinary pathname traversal would work. The
  // root-owned parent is readable, while the title-rule child remains private.
  auto wine = data.OpenChild("wine11", DirectoryPolicy::kApprovedTree, true, 0755);
  struct stat wine_stat{};
  if (!wine || fstat(wine->fd(), &wine_stat) != 0 || wine_stat.st_uid != geteuid() ||
      (wine_stat.st_mode & 07777) != 0755 ||
      !wine->Sync() || !data.Sync()) return {};
  auto rules = wine->OpenChild("title-rule", DirectoryPolicy::kPrivate, true);
  if (!rules || !rules->Sync() || !wine->Sync()) return {};
  int fd = rules->CreateExclusiveFile("wine11-title.lock", 0600);
  if (fd >= 0) {
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (!ok || !rules->Sync()) return {};
  } else {
    if (errno != EEXIST) return {};
    fd = rules->OpenRegularFile("wine11-title.lock", true, 0600);
    if (fd < 0) return {};
    struct stat info{};
    bool ok = fstat(fd, &info) == 0 && info.st_nlink == 1;
    if (close(fd) != 0) ok = false;
    if (!ok) return {};
  }
  return DedicatedTitleList::Open(std::move(*rules));
}
}
