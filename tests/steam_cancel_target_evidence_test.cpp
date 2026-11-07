#include "../src/steam_integration/cancel_target_evidence.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static std::atomic<bool> fail_sync{false};
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  if (fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/wine11-cancel-target-XXXXXX"; assert(mkdtemp(root));
  const int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher);
    assert(lease);
    const auto game = paths->games.path() / "440";
    assert(mkdir(game.c_str(), 0700) == 0);
    ww::steam_integration::CancelTargetEvidence evidence;
    assert(!ww::steam_integration::CaptureCancelTarget(paths->games, *lease, 440, 777, evidence));
    assert(mkdir((game / ".playden-download").c_str(), 0700) == 0);
    assert(ww::steam_integration::CaptureCancelTarget(paths->games, *lease, 440, 777, evidence));
    assert(ww::steam_integration::VerifyCancelTarget(paths->games, *lease, evidence));
    auto wrong = evidence; wrong.nonce++;
    assert(!ww::steam_integration::VerifyCancelTarget(paths->games, *lease, wrong));
    const auto old = game.string() + ".old";
    assert(rename(game.c_str(), old.c_str()) == 0 && mkdir(game.c_str(), 0700) == 0);
    assert(!ww::steam_integration::VerifyCancelTarget(paths->games, *lease, evidence));
    assert(rmdir(game.c_str()) == 0 && rename(old.c_str(), game.c_str()) == 0);
    assert(ww::steam_integration::VerifyCancelTarget(paths->games, *lease, evidence));
    const auto second = paths->games.path() / "441";
    assert(mkdir(second.c_str(), 0700) == 0 && mkdir((second / ".playden-download").c_str(), 0700) == 0);
    fail_sync = true;
    ww::steam_integration::CancelTargetEvidence failed;
    assert(!ww::steam_integration::CaptureCancelTarget(paths->games, *lease, 441, 888, failed));
    assert(std::filesystem::exists(second / ".wine11-cancel-intent"));
    assert(!ww::steam_integration::CaptureCancelTarget(paths->games, *lease, 441, 888, failed));
  }
  assert(close(anchor) == 0);
  std::filesystem::remove_all(root);
}
