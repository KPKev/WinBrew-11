// Regression test for cancellation cleanup visibility in the generated service.
#include "../src/steam_integration/downloads.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

namespace {
struct BlockingRunner : playden::installs::InstallRunner {
  std::atomic<bool> started{false};
  std::atomic<bool> removing{false};
  std::atomic<bool> release{false};
  bool fail_removal = false;

  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
                 const std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)>&,
                 const std::function<void(playden::installs::InstallStep)>&) override {
    started.store(true);
    while (!cancel.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override {
    removing.store(true);
    while (!release.load()) std::this_thread::sleep_for(1ms);
    return fail_removal ? "Cleanup failed." : std::string();
  }
};

template <class F> bool Wait(F predicate) {
  const auto until = std::chrono::steady_clock::now() + 5s;
  while (!predicate() && std::chrono::steady_clock::now() < until)
    std::this_thread::sleep_for(1ms);
  return predicate();
}
}

int main() {
  char directory[] = "/tmp/wine11-cancel-regression-XXXXXX";
  assert(mkdtemp(directory));
  for (bool fail : {false, true}) {
    BlockingRunner runner;
    runner.fail_removal = fail;
    playden::installs::InstallService service(runner, directory);
    ww::steam_integration::DownloadCoordinator coordinator(service);
    assert(coordinator.Install(440));
    assert(Wait([&] { return runner.started.load(); }));
    assert(Wait([&] { return !coordinator.Snapshot().jobs.empty(); }));
    assert(coordinator.Cancel(440));
    assert(Wait([&] { return runner.removing.load(); }));
    // Let the coordinator publish the post-cancel service snapshot. Its worker
    // polls every 250 ms; the empty queue shows the command was processed.
    assert(Wait([&] { return coordinator.Snapshot().jobs.empty(); }));
    const bool remained_busy = coordinator.Busy() && !coordinator.SafeToExit();
    runner.release.store(true);
    assert(remained_busy && "cleanup in progress must remain visible to exit guards");
    if (fail) {
      assert(Wait([&] { return coordinator.Error() == "Cleanup failed."; }));
      assert(coordinator.Busy() && !coordinator.SafeToExit());
      service.Install(playden::GameId::Steam(440));
      assert(service.Snapshot().jobs.empty());
      runner.fail_removal = false;
      assert(coordinator.Cancel(440));
      assert(Wait([&] { return coordinator.Error().empty() && !coordinator.Busy(); }));
      assert(coordinator.SafeToExit());
    } else {
      assert(Wait([&] { return !coordinator.Busy(); }));
      assert(coordinator.SafeToExit());
    }
  }
  {
    BlockingRunner runner;
    playden::installs::InstallService service(runner, directory);
    const auto first = playden::GameId::Steam(440);
    const auto second = playden::GameId::Steam(441);
    service.Install(first);
    assert(Wait([&] { return runner.started.load(); }));
    service.Install(second);
    service.Cancel(second);
    assert(service.Snapshot().cancel_cleanup.size() == 1);
    service.Install(second);
    assert(service.Snapshot().jobs.size() == 1);
    service.Pause(first);
    assert(Wait([&] { return runner.removing.load(); }));
    assert(service.Snapshot().cancel_cleanup.size() == 1);
    runner.release.store(true);
    assert(Wait([&] { return service.Snapshot().cancel_cleanup.empty(); }));
  }
  {
    BlockingRunner runner;
    std::vector<playden::installs::InstallVolume> volumes = {
      {"internal", "Console", directory, true}, {"external", "External", std::string(directory) + "/absent", false}};
    playden::installs::InstallService service(runner, volumes);
    const auto id = playden::GameId::Steam(442);
    service.Install(id, std::nullopt, "external");
    service.Cancel(id);
    const auto snapshot = service.Snapshot();
    assert(snapshot.cancel_cleanup_failures.size() == 1);
    assert(snapshot.cancel_cleanup_failures[0].reason.find("Reconnect") != std::string::npos);
    assert(snapshot.cancel_cleanup_retryable.empty());
  }
  std::filesystem::remove_all(directory);

  char recovery_dir[] = "/tmp/wine11-recovery-gate-XXXXXX";
  assert(mkdtemp(recovery_dir));
  { std::ofstream registry(std::string(recovery_dir) + "/.playden-games"); registry << "440\tinternal\t0\n"; }
  const std::vector<playden::installs::InstallVolume> recovery_volumes = {{"internal", "Console", recovery_dir, true}};
  {
    BlockingRunner runner;
    playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, recovery_volumes, [] { return false; });
    ww::steam_integration::DownloadCoordinator coordinator(service);
    assert(Wait([&] { return coordinator.Snapshot().recovery_blocked; }));
    assert(coordinator.Busy() && !coordinator.SafeToExit());
    assert(!coordinator.Error().empty());
    service.Install(playden::GameId::Steam(441));
    assert(service.Snapshot().jobs.empty() && !runner.started.load());
  }
  {
    BlockingRunner runner;
    playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, recovery_volumes,
      []() -> bool { throw 1; });
    assert(service.Snapshot().recovery_blocked && !runner.started.load());
  }
  {
    BlockingRunner runner;
    std::atomic<bool> entered{false}, release_gate{false};
    std::unique_ptr<playden::installs::InstallService> service;
    std::thread constructor([&] {
      service = std::make_unique<playden::installs::InstallService>(
        playden::installs::InstallService::Runners{{playden::Store::kSteam, &runner}}, recovery_volumes,
        [&] { entered = true; while (!release_gate.load()) std::this_thread::sleep_for(1ms); return true; });
    });
    assert(Wait([&] { return entered.load(); }));
    assert(!runner.started.load());
    release_gate = true;
    constructor.join();
    assert(Wait([&] { return runner.started.load(); }));
    assert(!service->Snapshot().recovery_blocked);
  }
  std::filesystem::remove_all(recovery_dir);
}
