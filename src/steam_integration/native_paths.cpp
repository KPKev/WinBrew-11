// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_paths.hpp"
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace ww::steam_integration {
namespace {
using playden::platform::ManagedDirectory;
using playden::platform::DirectoryPolicy;
using playden::platform::LockMode;
using playden::platform::ManagedFileLock;

bool EnsureLock(const ManagedDirectory& directory, const char* name) {
  int fd = directory.CreateExclusiveFile(name, 0600);
  if (fd >= 0) {
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok && directory.Sync();
  }
  if (errno != EEXIST) return false;
  fd = directory.OpenRegularFile(name, true, 0600);
  if (fd < 0) return false;
  struct stat info{};
  bool ok = fstat(fd, &info) == 0 && info.st_nlink == 1;
  if (close(fd) != 0) ok = false;
  return ok && directory.Validate();
}
}

std::optional<NativePaths> PrepareNativePaths(const char* rebound_root, int anchored_fd,
                                               int data_anchor, const char* data_path) {
  if (!rebound_root || anchored_fd < 0) return std::nullopt;
  struct stat anchor{}, named{};
  if (fstat(anchored_fd, &anchor) != 0 || !S_ISDIR(anchor.st_mode)) return std::nullopt;
  auto root = ManagedDirectory::Open(rebound_root, DirectoryPolicy::kApprovedTree);
  if (!root || fstat(root->fd(), &named) != 0 || anchor.st_dev != named.st_dev ||
      anchor.st_ino != named.st_ino || !root->Validate()) return std::nullopt;
  auto state = root->OpenChild("wine11-steam", DirectoryPolicy::kPrivate, true);
  if (!state || !state->Sync() || !root->Sync() || !EnsureLock(*state, "wine11-state.lock")) return std::nullopt;
  auto lease = ManagedFileLock::Acquire(*state, "wine11-state.lock", LockMode::kPublisher);
  if (!lease) return std::nullopt;
  std::optional<ManagedDirectory> games;
  if (data_anchor >= 0) {
    if (!data_path || !*data_path) return std::nullopt;
    struct stat data_anchored{}, data_observed{}, wine_info{};
    auto data = ManagedDirectory::Open(data_path, DirectoryPolicy::kApprovedTree);
    if (!data || data->path().filename() != "data" || fstat(data_anchor, &data_anchored) || fstat(data->fd(), &data_observed) ||
        !S_ISDIR(data_anchored.st_mode) || data_anchored.st_dev != data_observed.st_dev ||
        data_anchored.st_ino != data_observed.st_ino || !data->Validate()) return std::nullopt;
    auto wine = data->OpenChild("wine11", DirectoryPolicy::kApprovedTree, false);
    if (!wine || fstat(wine->fd(), &wine_info) || wine_info.st_uid != 0 ||
        (wine_info.st_mode & 07777) != 0755) return std::nullopt;
    games = wine->OpenChild("wine11-games", DirectoryPolicy::kPrivate, true);
    struct stat games_info{};
    if (!games || fstat(games->fd(), &games_info) || (games_info.st_mode & 07777) != 0700 ||
        !wine->Sync() || !data->Sync()) return std::nullopt;
  } else {
    games = state->OpenChild("wine11-games", DirectoryPolicy::kPrivate, true);
  }
  if (!games || !games->Sync() || !state->Sync() || !EnsureLock(*games, "wine11-install.lock") ||
      !state->Validate() || !lease->Validate(*state, "wine11-state.lock")) return std::nullopt;
  return NativePaths{std::move(*state), std::move(*lease), std::move(*games)};
}
std::optional<bool> HasSavedSteamSession(const NativePaths& paths) {
  if (!paths.state.Validate() || !paths.state_lease.Validate(paths.state, "wine11-state.lock")) return {};
  int fd = paths.state.OpenRegularFile("session.v1", true, 0600);
  if (fd < 0) {
    if (errno == ENOENT && paths.state.Validate() && paths.state_lease.Validate(paths.state, "wine11-state.lock")) return false;
    return {};
  }
  struct stat info{};
  const bool ok = fstat(fd, &info) == 0 && info.st_nlink == 1 && info.st_size > 0 && info.st_size <= 65536;
  const bool closed = close(fd) == 0;
  if (!ok || !closed || !paths.state.Validate() || !paths.state_lease.Validate(paths.state, "wine11-state.lock")) return {};
  return true;
}
}
