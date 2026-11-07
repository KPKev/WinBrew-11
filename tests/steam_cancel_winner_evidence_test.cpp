#include "../src/steam_integration/cancel_winner_evidence.hpp"
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
  char root[] = "/tmp/wine11-winner-evidence-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    const auto game = paths->games.path() / "440"; assert(mkdir(game.c_str(), 0700) == 0);
    const auto record = game / ".playden-install.json";
    int fd = open(record.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
    const std::string text = "{\"version\":1,\"app_id\":440,\"install_bytes\":123}\n";
    assert(write(fd, text.data(), text.size()) == static_cast<ssize_t>(text.size()) && close(fd) == 0);
    ww::steam_integration::CancelWinnerEvidence evidence;
    assert(ww::steam_integration::CaptureCancelWinner(paths->games, *lease, 440, evidence));
    assert(ww::steam_integration::VerifyCancelWinner(paths->games, *lease, evidence));
    auto wrong = evidence; wrong.app = 441;
    assert(!ww::steam_integration::VerifyCancelWinner(paths->games, *lease, wrong));
    const std::string bad = "{\"version\":1,\"app_id\":441}\n";
    fd = open(record.c_str(), O_WRONLY | O_TRUNC); assert(fd >= 0);
    assert(write(fd, bad.data(), bad.size()) == static_cast<ssize_t>(bad.size()) && close(fd) == 0);
    ww::steam_integration::CancelWinnerEvidence rejected;
    assert(!ww::steam_integration::CaptureCancelWinner(paths->games, *lease, 440, rejected));
    fd = open(record.c_str(), O_WRONLY | O_TRUNC); assert(fd >= 0);
    assert(write(fd, text.data(), text.size()) == static_cast<ssize_t>(text.size()) && close(fd) == 0);
    fail_sync = true;
    assert(!ww::steam_integration::CaptureCancelWinner(paths->games, *lease, 440, evidence));
    assert(std::filesystem::exists(record));
    const auto moved = game.string() + ".old";
    assert(rename(game.c_str(), moved.c_str()) == 0 && mkdir(game.c_str(), 0700) == 0);
    assert(!ww::steam_integration::VerifyCancelWinner(paths->games, *lease, evidence));
    assert(rmdir(game.c_str()) == 0 && rename(moved.c_str(), game.c_str()) == 0);
    assert(ww::steam_integration::VerifyCancelWinner(paths->games, *lease, evidence));
  }
  assert(close(anchor) == 0);
  std::filesystem::remove_all(root);
}
