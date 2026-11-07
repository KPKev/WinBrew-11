#include "../src/steam_integration/uninstall_quarantine.hpp"
#include "../src/steam_integration/native_paths.hpp"
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
static std::atomic<bool> arm{false}, fail_sync{false};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
  int result = __real_renameat(a, b, c, d);
  if (result == 0 && arm.exchange(false)) fail_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/vapor-quarantine-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    const auto game = paths->games.path() / "365670"; assert(mkdir(game.c_str(), 0700) == 0);
    std::ofstream(game / "keep") << "content";
    struct stat st{}; assert(paths->games.InspectChild("365670", &st));
    UninstallTarget t{365670, "11111111-2222-4333-8444-555555555555", static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)};
    auto invalid = t; invalid.operation = "00000000-0000-0000-0000-000000000000";
    assert(QuarantineInstalled(paths->games, *lease, invalid) == QuarantineState::blocked);
    auto wrong = t; ++wrong.inode;
    assert(QuarantineInstalled(paths->games, *lease, wrong) == QuarantineState::blocked);
    assert(std::filesystem::exists(game / "keep"));
    arm = true;
    assert(QuarantineInstalled(paths->games, *lease, t) == QuarantineState::blocked);
    assert(!std::filesystem::exists(game));
    assert(InspectQuarantine(paths->games, *lease, t) == QuarantineState::moved);
    assert(QuarantineInstalled(paths->games, *lease, t) == QuarantineState::moved);
    assert(std::filesystem::exists(paths->games.path() / ".vapor-retired" / t.operation / "keep"));
    // A replacement game folder can never be mistaken for the quarantined one.
    assert(mkdir(game.c_str(), 0700) == 0);
    assert(InspectQuarantine(paths->games, *lease, t) == QuarantineState::blocked);
    assert(std::filesystem::exists(game));
  }
  close(anchor); std::filesystem::remove_all(root);
}
