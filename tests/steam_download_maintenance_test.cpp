#include "../src/steam_integration/downloads.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<bool> started{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    started = true; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { assert(false); return {}; }
};
template<class F> void Wait(F f) {
  const auto end = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  char root[] = "/tmp/vapor-maintenance-XXXXXX"; assert(mkdtemp(root));
  { std::ofstream registry(std::filesystem::path(root) / ".playden-games"); registry << "730\tinternal\t0\tpaused\n"; }
  {
    Runner runner; playden::installs::InstallService service(runner, root);
    ww::steam_integration::DownloadCoordinator coordinator(service);
    Wait([&] { return coordinator.SafeToExit(); });
    assert(coordinator.Snapshot().jobs.size() == 1);
    assert(coordinator.Snapshot().jobs[0].state == playden::installs::InstallJobState::kPaused);
    assert(coordinator.MaintenanceReady());
    assert(coordinator.FreezeForMaintenance() && coordinator.FrozenForMaintenance());
    assert(coordinator.Busy() && !coordinator.SafeToExit());
    std::atomic<bool> accepted{false};
    std::thread concurrent([&] { for (int i = 0; i < 100; ++i) if (coordinator.Install(440)) accepted = true; });
    concurrent.join(); assert(!accepted && !runner.started);
    assert(!coordinator.FreezeForMaintenance());
    assert(coordinator.ThawAfterMaintenance() && !coordinator.FrozenForMaintenance());
    assert(coordinator.Install(440)); Wait([&] { return runner.started.load(); });
    assert(!coordinator.FreezeForMaintenance());
  }
  std::filesystem::remove_all(root);
}
