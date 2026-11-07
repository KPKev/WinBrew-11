#include "../src/steam_integration/cancel_cleanup_journal.hpp"
#include "../src/steam_integration/cancel_registry_reconciliation.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include "installs/install_service.h"
#include "platform/ps5/title_allow_list.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<bool> entered{false}; std::atomic<int> removed{0};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    const auto uid = playden::ps5::HoldEffectiveUid(); entered = true;
    while (!cancel.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { ++removed; return {}; }
};
template<class F> void Wait(F f) {
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  char root[] = "/tmp/wine11-quiesce-journal-XXXXXX"; assert(mkdtemp(root));
  const int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    const auto game = paths->games.path() / "440";
    assert(mkdir(game.c_str(), 0700) == 0 && mkdir((game / ".playden-download").c_str(), 0700) == 0);
    ww::steam_integration::CancelCleanupJournal journal(paths->state, paths->state_lease, paths->games, *lease);
    using State = ww::steam_integration::CancelCleanupJournal::State;
    Runner runner;
    const std::vector<playden::installs::InstallVolume> volumes = {{"internal", "Console", paths->games.path().string(), true}};
    {
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, volumes, [] { return true; },
        [&](playden::GameId id) {
          const auto uid = playden::ps5::HoldEffectiveUid();
          ww::steam_integration::CancelTargetEvidence target;
          return id == playden::GameId::Steam(440) &&
            ww::steam_integration::InspectCancelTarget(paths->games, *lease, 440, 222, target) &&
            journal.Prepare(440, 111, target) && journal.CaptureMarker(440, 111) && journal.BindMarker(440, 111);
        });
      service.Install(playden::GameId::Steam(440)); Wait([&] { return runner.entered.load(); });
      service.Cancel(playden::GameId::Steam(440));
      Wait([&] { return journal.Read().state == State::intent; });
      const auto snapshot = service.Snapshot();
      assert(snapshot.jobs.size() == 1 && snapshot.cancel_cleanup.size() == 1 && runner.removed == 0);
    }
    {
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, volumes, [&] {
        assert(journal.Read().state == State::intent);
        assert(ww::steam_integration::ReconcileCanceledQueueBeforeScan(journal, paths->games, *lease));
        return false;
      });
      assert(service.Snapshot().recovery_blocked && runner.removed == 0 && journal.Read().state == State::intent);
    }
    assert(std::filesystem::exists(game / ".playden-download"));
  }
  assert(close(anchor) == 0);
  std::filesystem::remove_all(root);
}
