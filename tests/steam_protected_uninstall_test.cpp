#include "../src/steam_integration/protected_uninstall.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
static std::atomic<bool> fail_after_unlink{false}, fail_sync{false};
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* name, int flags) {
  int result = __real_unlinkat(fd, name, flags);
  if (result == 0 && fail_after_unlink.exchange(false)) fail_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  assert(geteuid() == 0);
  const char* user = std::getenv("SUDO_UID"); const uid_t app_uid = user ? static_cast<uid_t>(std::strtoul(user, nullptr, 10)) : 1000;
  assert(app_uid != 0);
  char root[] = "/tmp/vapor-protected-XXXXXX"; assert(mkdtemp(root)); assert(chmod(root, 0755) == 0);
  const auto base = std::filesystem::path(root), games_path = base / "wine11-games";
  assert(mkdir(games_path.c_str(), 0700) == 0 && chown(games_path.c_str(), app_uid, -1) == 0);
  { std::ofstream out(base / "outside"); out << "preserve"; }
  assert(seteuid(app_uid) == 0);
  int lockfd = open((games_path / "wine11-install.lock").c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600); assert(lockfd >= 0); close(lockfd);
  auto games = playden::platform::ManagedDirectory::Open(games_path, playden::platform::DirectoryPolicy::kPrivate); assert(games);
  auto lease = playden::platform::ManagedFileLock::Acquire(*games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
  const auto game = games_path / "365670"; assert(mkdir(game.c_str(), 0700) == 0 && mkdir((game / "nested").c_str(), 0700) == 0);
  { std::ofstream out(game / "nested" / "file"); out << "data"; }
  assert(symlink((base / "outside").c_str(), (game / "link").c_str()) == 0);
  struct stat st{}; assert(games->InspectChild("365670", &st));
  UninstallTarget t{365670, "11111111-2222-4333-8444-555555555555", static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)};
  assert(InspectProtectedGame(*games, *lease, base, t) == ProtectedState::original);
  auto wrong = t; ++wrong.inode;
  assert(RetireProtectedGame(*games, *lease, base, wrong) == ProtectedState::blocked);
  assert(RetireProtectedGame(*games, *lease, base, t) == ProtectedState::retired);
  errno = 0; int unauthorized = open((base / ".vapor-uninstall" / t.operation).c_str(), O_RDONLY | O_DIRECTORY);
  assert(unauthorized == -1 && errno == EACCES);
  // Simulate a crash after the bridge was opened; inspection reseals it.
  assert(seteuid(0) == 0 && chmod((base / ".vapor-uninstall").c_str(), 0733) == 0 && seteuid(app_uid) == 0);
  assert(InspectProtectedGame(*games, *lease, base, t) == ProtectedState::retired);
  assert(seteuid(0) == 0 && lstat((base / ".vapor-uninstall").c_str(), &st) == 0 && (st.st_mode & 07777) == 0700 && seteuid(app_uid) == 0);
  // A deletion may occur before a sync reports failure; the same identity can
  // be reopened and the remaining entries completed on recovery.
  fail_after_unlink = true;
  assert(RemoveProtectedGame(*games, *lease, base, t) == ProtectedState::blocked);
  assert(InspectProtectedGame(*games, *lease, base, t) == ProtectedState::retired);
  assert(RemoveProtectedGame(*games, *lease, base, t) == ProtectedState::removed);
  assert(InspectProtectedGame(*games, *lease, base, t) == ProtectedState::removed);
  assert(std::filesystem::exists(base / "outside"));
  lease.reset(); games.reset(); assert(seteuid(0) == 0); std::filesystem::remove_all(base);
}
