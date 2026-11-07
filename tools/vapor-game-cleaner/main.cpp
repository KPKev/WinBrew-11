// SPDX-License-Identifier: GPL-3.0-or-later
// One-shot loader payload. Only a previously retired, identity-matched game
// inside the root-sealed uninstall directory can be emptied.
#include "protocol.hpp"
#include "steam_integration/safe_remove_tree.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using vapor_cleaner::Request;
bool Missing(const std::string& path) {
  struct stat st{}; errno = 0;
  return lstat(path.c_str(), &st) != 0 && errno == ENOENT;
}
bool Dir(const std::string& path, uid_t owner, mode_t mode, struct stat* out = nullptr) {
  struct stat st{};
  if (lstat(path.c_str(), &st) || !S_ISDIR(st.st_mode) || st.st_uid != owner || (st.st_mode & 07777) != mode) return false;
  if (out) *out = st;
  return true;
}
bool Root(const std::string& path, struct stat& st) {
  return !lstat(path.c_str(), &st) && S_ISDIR(st.st_mode) && st.st_uid == 0 && !(st.st_mode & 0022);
}
bool SyncDir(const std::string& path) {
  const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  const bool ok = fsync(fd) == 0;
  return close(fd) == 0 && ok;
}
bool ReadRegular(const std::string& path, std::size_t limit, std::string& text, bool registry = false) {
  const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  struct stat before{}, after{}, named{};
  bool ok = !fstat(fd, &before) && S_ISREG(before.st_mode) && before.st_uid == 1 && before.st_nlink == 1 &&
      before.st_size >= 0 && static_cast<std::uint64_t>(before.st_size) <= limit &&
      ((before.st_mode & 07777) == 0600 || (registry && ((before.st_mode & 07777) == 0644 || (before.st_mode & 07777) == 0666)));
  if (ok) text.resize(static_cast<std::size_t>(before.st_size));
  std::size_t done = 0;
  while (ok && done < text.size()) {
    const auto n = read(fd, &text[done], text.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  ok = ok && !fstat(fd, &after) && !lstat(path.c_str(), &named) && before.st_dev == after.st_dev &&
       before.st_ino == after.st_ino && before.st_size == after.st_size && before.st_uid == after.st_uid &&
       before.st_mode == after.st_mode && before.st_nlink == after.st_nlink &&
       before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
       before.st_ctim.tv_sec == after.st_ctim.tv_sec && before.st_ctim.tv_nsec == after.st_ctim.tv_nsec &&
       named.st_dev == after.st_dev && named.st_ino == after.st_ino && named.st_size == after.st_size &&
       named.st_uid == after.st_uid && named.st_mode == after.st_mode && named.st_nlink == after.st_nlink &&
       named.st_mtim.tv_sec == after.st_mtim.tv_sec && named.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
       named.st_ctim.tv_sec == after.st_ctim.tv_sec && named.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
  return close(fd) == 0 && ok;
}
bool RegistryAbsent(const std::string& games, std::uint32_t app) {
  std::string text;
  if (!ReadRegular(games + "/.playden-games", 1024 * 1024, text, true) || (!text.empty() && text.back() != '\n')) return false;
  const std::string key = std::to_string(app) + "\t";
  for (std::size_t i = 0; i < text.size();) {
    const auto end = text.find('\n', i);
    if (end == std::string::npos || text.compare(i, key.size(), key) == 0) return false;
    i = end + 1;
  }
  return true;
}
bool SameGame(const struct stat& st, const Request& r) {
  return S_ISDIR(st.st_mode) && st.st_uid == 1 && static_cast<std::uint64_t>(st.st_dev) == r.device &&
      static_cast<std::uint64_t>(st.st_ino) == r.inode;
}
int OpenSealedGame(const std::string& parent, const std::string& target, const Request& request) {
  // UID 0 on the console cannot open a UID 1/0700 directory. Briefly allow
  // traversal through the root-owned quarantine, open the known inode as UID 1,
  // and reseal the parent before touching any contents. An interrupted 0733
  // bridge is already detected and resealed by InspectProtectedGame.
  if (!Dir(parent, 0, 0700) || chmod(parent.c_str(), 0733)) return -1;
  int fd = -1; struct stat opened{};
  if (!seteuid(1)) {
    fd = open(target.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd >= 0 && (fstat(fd, &opened) || !SameGame(opened, request))) { close(fd); fd = -1; }
  }
  const bool root = seteuid(0) == 0;
  const bool sealed = root && chmod(parent.c_str(), 0700) == 0 && Dir(parent, 0, 0700);
  if (!sealed && fd >= 0) { close(fd); fd = -1; }
  return fd;
}
bool PutResult(const std::string& games, const std::string& request) {
  const auto path = games + "/" + vapor_cleaner::kResult;
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  const std::string result = request + "ok\n";
  std::size_t done = 0; bool ok = fchmod(fd, 0600) == 0;
  while (ok && done < result.size()) {
    const auto n = write(fd, result.data() + done, result.size() - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    done += static_cast<std::size_t>(n);
  }
  ok = ok && !fsync(fd);
  return close(fd) == 0 && ok && SyncDir(games);
}
int Run(const std::string& data, const std::string& homebrew, const std::string& apps) {
  const auto games = data + "/wine11-games", parent = data + "/.vapor-uninstall";
  struct stat data_st{}, games_st{}, parent_st{}, target_st{}, opened{};
  if (seteuid(0) || !Root(data, data_st) || !Dir(parent, 0, 0700, &parent_st) || data_st.st_dev != parent_st.st_dev ||
      seteuid(1) || !Dir(games, 1, 0700, &games_st) || data_st.st_dev != games_st.st_dev ||
      !Missing(games + "/" + vapor_cleaner::kResult)) return 1;
  std::string text; Request request;
  if (!ReadRegular(games + "/" + vapor_cleaner::kRequest, 256, text) || !vapor_cleaner::Decode(text, request) ||
      request.device != static_cast<std::uint64_t>(games_st.st_dev) || !Missing(games + "/" + std::to_string(request.app)) ||
      !RegistryAbsent(games, request.app) || seteuid(0) || !Root(data, data_st) || !Dir(parent, 0, 0700, &parent_st) ||
      !Missing(homebrew + "/" + request.title) || !Missing(apps + "/" + request.title)) return 2;
  const auto target = parent + "/" + request.operation;
  if (lstat(target.c_str(), &target_st) || !SameGame(target_st, request)) return 3;
  const int fd = OpenSealedGame(parent, target, request);
  if (fd < 0) return 4;
  if (fstat(fd, &opened) || !SameGame(opened, request) || seteuid(1)) { close(fd); return 5; }
  const bool emptied = ww::steam_integration::safe_remove_detail::empty_dir(fd, opened.st_dev, 1, 0) && !fsync(fd);
  const bool closed = close(fd) == 0;
  if (seteuid(0) || !emptied || !closed || !Dir(parent, 0, 0700, &parent_st) ||
      parent_st.st_dev != data_st.st_dev || lstat(target.c_str(), &target_st) || !SameGame(target_st, request) ||
      rmdir(target.c_str()) || !SyncDir(parent) || !Missing(target)) return 6;
  std::string current;
  if (seteuid(1) || !Dir(games, 1, 0700) || !ReadRegular(games + "/" + vapor_cleaner::kRequest, 256, current) ||
      current != text || !Missing(games + "/" + std::to_string(request.app)) || !RegistryAbsent(games, request.app) ||
      !PutResult(games, text)) return 7;
  return 0;
}
}  // namespace
int main(int argc, char** argv) {
#if defined(VAPOR_CLEANER_TESTING)
  if (argc != 2) return 99;
  const std::string root = argv[1];
  return Run(root + "/wine11", root + "/homebrew", root + "/app");
#else
  (void)argc; (void)argv;
  return Run("/data/wine11", "/data/homebrew", "/user/app");
#endif
}
