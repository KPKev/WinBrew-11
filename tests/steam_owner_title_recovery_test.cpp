#include "../src/steam_integration/owner_title_recovery.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
static std::atomic<bool> fail_after_unlink{false}, fail_sync{false};
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* name, int flags) {
  const int result = __real_unlinkat(fd, name, flags);
  if (!result && fail_after_unlink.exchange(false)) fail_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (!fstat(fd, &st) && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  assert(geteuid() == 0);
  const char* user = std::getenv("SUDO_UID"); const uid_t app_uid = user ? static_cast<uid_t>(std::strtoul(user, nullptr, 10)) : 1000;
  assert(app_uid != 0);
  char dir[] = "/tmp/vapor-title-recovery-XXXXXX"; assert(mkdtemp(dir)); assert(chmod(dir, 0755) == 0);
  const auto base = std::filesystem::path(dir), data = base / "data", homebrew = base / "homebrew", games_path = data / "wine11-games";
  assert(mkdir(data.c_str(), 0755) == 0 && mkdir(homebrew.c_str(), 0777) == 0 && chmod(homebrew.c_str(), 0777) == 0);
  assert(mkdir(games_path.c_str(), 0700) == 0 && chown(games_path.c_str(), app_uid, -1) == 0);
  const auto quarantine = data / ".title-quarantine";
  assert(mkdir(quarantine.c_str(), 0700) == 0);
  assert(mkdir((quarantine / "title").c_str(), 0700) == 0 && chown((quarantine / "title").c_str(), app_uid, -1) == 0);
  struct stat st{}; assert(lstat((quarantine / "title").c_str(), &st) == 0);
  OwnerTitleRecoveryTarget target{data, homebrew, ".title-quarantine", "FAKE78008", ".receipt", "authorized\n", 365670, st.st_dev, st.st_ino};
  { std::ofstream out(base / "outside"); out << "preserve"; }
  assert(chmod(quarantine.c_str(), 0733) == 0 && seteuid(app_uid) == 0);
  assert(mkdir((quarantine / "title" / "nested").c_str(), 0700) == 0);
  { std::ofstream out(quarantine / "title" / "nested" / "file"); out << "data"; }
  assert(symlink((base / "outside").c_str(), (quarantine / "title" / "link").c_str()) == 0);
  assert(seteuid(0) == 0 && chmod(quarantine.c_str(), 0700) == 0 && seteuid(app_uid) == 0);
  int lockfd = open((games_path / "wine11-install.lock").c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600); assert(lockfd >= 0 && close(lockfd) == 0);
  auto games = playden::platform::ManagedDirectory::Open(games_path, playden::platform::DirectoryPolicy::kPrivate); assert(games);
  auto lease = playden::platform::ManagedFileLock::Acquire(*games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
  { int fd = open((games_path / target.receipt_name).c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    assert(fd >= 0 && write(fd, target.receipt_text.data(), target.receipt_text.size()) == static_cast<ssize_t>(target.receipt_text.size()) && close(fd) == 0); }
  UninstallJournal::Entry entry; entry.phase = UninstallJournal::Phase::intent; entry.app = 365670;
  entry.title = "FAKE78008"; entry.operation = "11111111-2222-4333-8444-555555555555";
  const auto now = std::time(nullptr);
  auto wrong = target; ++wrong.inode;
  assert(!ClaimOwnerTitleRecovery(*games, *lease, entry, wrong, now));
  auto invalid = entry; invalid.reinstall = true; assert(!ClaimOwnerTitleRecovery(*games, *lease, invalid, target, now));
  assert(!ClaimOwnerTitleRecovery(*games, *lease, entry, target, now + 601));
  assert(mkdir((homebrew / target.title).c_str(), 0700) == 0);
  assert(!ClaimOwnerTitleRecovery(*games, *lease, entry, target, now));
  assert(rmdir((homebrew / target.title).c_str()) == 0);
  assert(ClaimOwnerTitleRecovery(*games, *lease, entry, target, now));
  assert(!ClaimOwnerTitleRecovery(*games, *lease, entry, target, now));
  fail_after_unlink = true;
  assert(!CleanupOwnerTitleQuarantine(*games, *lease, target));
  assert(seteuid(0) == 0 && lstat(quarantine.c_str(), &st) == 0 && (st.st_mode & 07777) == 0700 && seteuid(app_uid) == 0);
  assert(CleanupOwnerTitleQuarantine(*games, *lease, target));
  assert(OwnerTitleQuarantineAbsent(*games, *lease, target));
  assert(std::filesystem::exists(base / "outside"));
  lease.reset(); games.reset(); assert(seteuid(0) == 0); std::filesystem::remove_all(base);
}
