// SPDX-License-Identifier: GPL-3.0-or-later
#include "owner_cleanup_probe.hpp"
#include "protected_uninstall.hpp"
#include "uninstall_registry.hpp"
#include "platform/ps5/title_allow_list.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
namespace ww::steam_integration {
namespace {
constexpr char kGame[] = "4294967286";
constexpr char kOperation[] = "47fc159f-096c-4399-a934-4455637be846";
constexpr char kReport[] = ".vapor-cleanup-probe-47fc159f-096c-4399-a934-4455637be846.txt";
constexpr char kSyscallReport[] = ".vapor-cleanup-syscalls-47fc159f-096c-4399-a934-4455637be846.txt";
bool Put(const std::filesystem::path& path) {
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  const bool written = write(fd, "x", 1) == 1 && fsync(fd) == 0;
  return close(fd) == 0 && written;
}
bool Report(const playden::platform::ManagedDirectory& games, const std::string& name, const std::string& report) {
  const int fd = games.CreateExclusiveFile(name, 0600); if (fd < 0) return false;
  std::size_t n = 0; bool ok = true;
  while (n < report.size()) {
    const auto count = write(fd, report.data() + n, report.size() - n);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) { ok = false; break; }
    n += static_cast<std::size_t>(count);
  }
  const bool synced = ok && fsync(fd) == 0; const bool closed = close(fd) == 0;
  return synced && closed && games.Sync();
}
}
bool RunOwnerCleanupProbe(const playden::platform::ManagedDirectory& games,
                          const playden::platform::ManagedFileLock& lease,
                          const std::filesystem::path& data_parent) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!data_parent.is_absolute() || !games.Validate() || !lease.Validate(games, "wine11-install.lock") ||
      games.path() != data_parent / "wine11-games") return false;
  struct stat st{}; errno = 0;
  if (games.InspectChild(kReport, &st) || errno != ENOENT) return false;
  errno = 0; if (games.InspectChild(kSyscallReport, &st) || errno != ENOENT) return false;
  errno = 0; if (games.InspectChild(kGame, &st) || errno != ENOENT) return false;
  const auto registry = ReadInstalledRegistry(games, lease);
  if (!registry || registry->find(std::string(kGame) + "\t") != std::string::npos) return false;
  const auto source = games.path() / kGame;
  if (mkdir(source.c_str(), 0700) || !games.InspectChild(kGame, &st) || !S_ISDIR(st.st_mode) || st.st_uid != geteuid()) return false;
  const auto device = static_cast<std::uint64_t>(st.st_dev), inode = static_cast<std::uint64_t>(st.st_ino);
  if (!Put(source / "wrapped") || mkdir((source / "nested").c_str(), 0700) ||
      !Put(source / "nested" / "keep") || !games.Sync()) return false;
  int fd = open(source.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return false;
  std::string out = "fixture_app=" + std::string(kGame) + "\nfixture_operation=" + kOperation + "\nfixture_inode=" + std::to_string(inode) + "\n";
  errno = 0; const int wrapped_stat = fstatat(fd, "wrapped", &st, AT_SYMLINK_NOFOLLOW); const int wrapped_stat_errno = errno;
  errno = 0; const int wrapped_unlink = unlinkat(fd, "wrapped", 0); const int wrapped_unlink_errno = errno;
  out += "wrapped_stat=" + std::to_string(wrapped_stat) + " errno=" + std::to_string(wrapped_stat_errno) + "\n";
  out += "wrapped_unlink=" + std::to_string(wrapped_unlink) + " errno=" + std::to_string(wrapped_unlink_errno) + "\n";
  if (!Report(games, kSyscallReport, out + "syscall_probe_complete=1\n")) { close(fd); return false; }
  if (fsync(fd) || close(fd)) return false;
  const UninstallTarget target{4294967286U, kOperation, device, inode};
  errno = 0; const auto retired = RetireProtectedGame(games, lease, data_parent, target); const int retire_errno = errno;
  out += "retire_state=" + std::to_string(static_cast<unsigned>(retired)) + " errno=" + std::to_string(retire_errno) + "\n";
  if (retired == ProtectedState::retired) {
    errno = 0; const auto removed = RemoveProtectedGame(games, lease, data_parent, target); const int remove_errno = errno;
    out += "remove_state=" + std::to_string(static_cast<unsigned>(removed)) + " errno=" + std::to_string(remove_errno) + "\n";
  }
  out += "probe_complete=1\n";
  return Report(games, kReport, out);
}
}
