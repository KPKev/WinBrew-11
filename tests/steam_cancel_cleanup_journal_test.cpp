#include "../src/steam_integration/cancel_cleanup_journal.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include "../src/steam_integration/cancel_registry_reconciliation.hpp"
#include "installs/install_service.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

static std::atomic<int> directory_syncs{0}, fail_at{0};
static std::atomic<bool> arm_after_rename{false}, fail_next_directory_sync{false};
static std::atomic<bool> fail_next_file_sync{false};
struct QuietRunner : playden::installs::InstallRunner {
  std::atomic<int> starts{0};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>&,
      const std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    ++starts; return {};
  }
  std::string Remove(const std::string&) override { assert(false); return {}; }
};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
  const int result = __real_renameat(a, b, c, d);
  if (result == 0 && arm_after_rename.exchange(false)) fail_next_directory_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat info{};
  if (fail_next_file_sync.load() && fstat(fd, &info) == 0 && S_ISREG(info.st_mode)) {
    char link[64], path[512]{};
    std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    const auto n = readlink(link, path, sizeof(path) - 1);
    if (n > 0 && std::strstr(path, ".wine11-cancel-intent") && fail_next_file_sync.exchange(false)) { errno = EIO; return -1; }
  }
  if (fstat(fd, &info) == 0 && S_ISDIR(info.st_mode)) {
    if (fail_next_directory_sync.exchange(false)) { errno = EIO; return -1; }
    const int count = ++directory_syncs;
    if (fail_at.load() == count) { errno = EIO; return -1; }
  }
  return __real_fsync(fd);
}

int main() {
  char root[] = "/tmp/wine11-cleanup-journal-XXXXXX"; assert(mkdtemp(root));
  const int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher);
    assert(lease);
    using Journal = ww::steam_integration::CancelCleanupJournal;
    Journal journal(paths->state, paths->state_lease, paths->games, *lease);
    const auto target = paths->games.path() / "440";
    assert(mkdir(target.c_str(), 0700) == 0 && mkdir((target / ".playden-download").c_str(), 0700) == 0);
    ww::steam_integration::CancelTargetEvidence evidence;
    assert(ww::steam_integration::InspectCancelTarget(paths->games, *lease, 440, 555, evidence));
    assert(journal.Read().state == Journal::State::absent);
    // Absence is synced first; fail the directory sync following publication.
    directory_syncs = 0; fail_at = 2;
    assert(!journal.Prepare(440, 123, evidence));
    fail_at = 0;
    auto entry = journal.Read();
    assert(entry.state == Journal::State::prepared && entry.app == 440 && entry.operation == 123);
    {
      QuietRunner runner;
      bool attempted_reconcile = false;
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}},
          {{"internal", "Console", paths->games.path().string(), true}}, [&] {
            const auto pending = journal.Read();
            if (pending.state == Journal::State::intent) {
              attempted_reconcile = true;
              (void)ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease);
            }
            return false;
          });
      assert(service.Snapshot().recovery_blocked && runner.starts == 0 && !attempted_reconcile);
    }
    assert(!journal.BindMarker(440, 123));
    const auto replaced = target.string() + ".before-marker";
    assert(rename(target.c_str(), replaced.c_str()) == 0 && mkdir(target.c_str(), 0700) == 0);
    assert(mkdir((target / ".playden-download").c_str(), 0700) == 0);
    assert(!journal.CaptureMarker(440, 123));
    assert(rmdir((target / ".playden-download").c_str()) == 0 && rmdir(target.c_str()) == 0);
    assert(rename(replaced.c_str(), target.c_str()) == 0);
    // Failed marker sync may leave a fully written marker. It remains prepared
    // until a new durable verification succeeds; no deletion is permitted.
    fail_next_file_sync = true;
    assert(!journal.CaptureMarker(440, 123));
    assert(journal.Read().state == Journal::State::prepared);
    arm_after_rename = true;
    assert(!journal.BindMarker(440, 123));
    entry = journal.Read(); assert(entry.state == Journal::State::intent);
    playden::Storage registry(paths->games.path().string());
    const std::string queued = "100\tinternal\t30\n440\tinternal\t0\tpaused\n441\tinternal\t0\n";
    assert(registry.WriteDurable(".playden-games", {queued.begin(), queued.end()}).ok());
    const auto completed_record = target / ".playden-install.json";
    int complete_fd = open(completed_record.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(complete_fd >= 0);
    assert(close(complete_fd) == 0);
    assert(!ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease));
    auto unchanged = registry.ReadDurable(".playden-games"); assert(unchanged.ok());
    assert(std::string(unchanged.value().begin(), unchanged.value().end()) == queued);
    assert(unlink(completed_record.c_str()) == 0);
    arm_after_rename = true;
    assert(!ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease));
    assert(ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease));
    auto reconciled = registry.ReadDurable(".playden-games"); assert(reconciled.ok());
    assert(std::string(reconciled.value().begin(), reconciled.value().end()) == "100\tinternal\t30\n441\tinternal\t0\n");
    assert(registry.WriteDurable(".playden-games", {queued.begin(), queued.end()}).ok());
    QuietRunner runner;
    bool gate_called = false, gate_reconciled = false;
    {
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}},
          {{"internal", "Console", paths->games.path().string(), true}}, [&] {
            gate_called = true;
            gate_reconciled = ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease);
            return false; // deletion and complete recovery are not implemented.
          });
      assert(gate_called && gate_reconciled && service.Snapshot().recovery_blocked && runner.starts == 0);
    }
    assert(!journal.Prepare(441, 124, evidence));
    assert(!journal.Complete(440, 124));
    assert(!journal.Complete(440, 123));
    Journal restarted(paths->state, paths->state_lease, paths->games, *lease);
    entry = restarted.Read();
    assert(entry.state == Journal::State::intent && entry.app == 440 && entry.operation == 123);
    assert(!restarted.Prepare(441, 124, evidence));
    const auto lock = paths->state.path() / "wine11-state.lock";
    const auto renamed = paths->state.path() / "wine11-state.lock.test";
    assert(rename(lock.c_str(), renamed.c_str()) == 0);
    assert(restarted.Read().state == Journal::State::blocked);
    assert(rename(renamed.c_str(), lock.c_str()) == 0);
    const auto games = paths->games.path();
    const auto moved = games.string() + ".test";
    assert(rename(games.c_str(), moved.c_str()) == 0);
    assert(mkdir(games.c_str(), 0700) == 0);
    assert(restarted.Read().state == Journal::State::blocked);
    assert(rmdir(games.c_str()) == 0);
    assert(rename(moved.c_str(), games.c_str()) == 0);
    const auto moved_target = target.string() + ".test";
    assert(rename(target.c_str(), moved_target.c_str()) == 0 && mkdir(target.c_str(), 0700) == 0);
    assert(restarted.Read().state == Journal::State::blocked);
    assert(rmdir(target.c_str()) == 0 && rename(moved_target.c_str(), target.c_str()) == 0);
    const auto file = paths->state.path() / "cancel-cleanup.v1";
    const int corrupt = open(file.c_str(), O_WRONLY | O_TRUNC); assert(corrupt >= 0);
    assert(write(corrupt, "invalid\n", 8) == 8 && close(corrupt) == 0);
    assert(restarted.Read().state == Journal::State::blocked);
  }
  assert(close(anchor) == 0);
  std::filesystem::remove_all(root);

  char race_root[] = "/tmp/wine11-cleanup-journal-race-XXXXXX"; assert(mkdtemp(race_root));
  const int race_anchor = open(race_root, O_RDONLY | O_DIRECTORY); assert(race_anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(race_root, race_anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher);
    assert(lease);
    using Journal = ww::steam_integration::CancelCleanupJournal;
    Journal first(paths->state, paths->state_lease, paths->games, *lease);
    Journal second(paths->state, paths->state_lease, paths->games, *lease);
    ww::steam_integration::CancelTargetEvidence first_target, second_target;
    for (const char* app : {"440", "441"}) {
      const auto target = paths->games.path() / app;
      assert(mkdir(target.c_str(), 0700) == 0 && mkdir((target / ".playden-download").c_str(), 0700) == 0);
    }
    assert(ww::steam_integration::InspectCancelTarget(paths->games, *lease, 440, 1000, first_target));
    assert(ww::steam_integration::InspectCancelTarget(paths->games, *lease, 441, 1001, second_target));
    std::atomic<bool> go{false}; bool a = false, b = false;
    std::thread one([&] { while (!go.load()) {} a = first.Prepare(440, 100, first_target); });
    std::thread two([&] { while (!go.load()) {} b = second.Prepare(441, 101, second_target); });
    go = true; one.join(); two.join();
    assert(a != b);
    const auto observed = first.Read();
    assert(observed.state == Journal::State::prepared);
    assert((a && observed.app == 440 && observed.operation == 100) ||
           (b && observed.app == 441 && observed.operation == 101));
  }
  assert(close(race_anchor) == 0);
  std::filesystem::remove_all(race_root);
  char winner_root[] = "/tmp/wine11-winner-journal-XXXXXX"; assert(mkdtemp(winner_root));
  const int winner_anchor = open(winner_root, O_RDONLY | O_DIRECTORY); assert(winner_anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(winner_root, winner_anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    const auto game = paths->games.path() / "440"; assert(mkdir(game.c_str(), 0700) == 0);
    const auto record = game / ".playden-install.json";
    int fd = open(record.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
    const std::string content = "{\"version\":1,\"app_id\":440}\n";
    assert(write(fd, content.data(), content.size()) == static_cast<ssize_t>(content.size()) && close(fd) == 0);
    ww::steam_integration::CancelWinnerEvidence winner;
    assert(ww::steam_integration::CaptureCancelWinner(paths->games, *lease, 440, winner));
    using Journal = ww::steam_integration::CancelCleanupJournal;
    Journal journal(paths->state, paths->state_lease, paths->games, *lease);
    assert(!journal.RecordWinner(440, 999, winner));
    Journal restarted(paths->state, paths->state_lease, paths->games, *lease);
    auto observed = restarted.Read();
    assert(observed.state == Journal::State::absent);
    fd = open(record.c_str(), O_WRONLY | O_TRUNC); assert(fd >= 0);
    assert(write(fd, "bad", 3) == 3 && close(fd) == 0);
    assert(restarted.Read().state == Journal::State::absent);
  }
  assert(close(winner_anchor) == 0);
  std::filesystem::remove_all(winner_root);
}
