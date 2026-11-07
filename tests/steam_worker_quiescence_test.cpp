#include "../src/steam_integration/downloads.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<int> starts{0};
  std::atomic<bool> stopping{false}, release_install{false}, removing{false}, release_remove{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    ++starts; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    stopping = true; while (!release_install.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override {
    removing = true; while (!release_remove.load()) std::this_thread::sleep_for(1ms); return {};
  }
};
template<class F> void Wait(F f) {
  const auto end = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  for (bool cancel : {false, true}) {
    char path[] = "/tmp/wine11-worker-idle-XXXXXX"; assert(mkdtemp(path));
    {
      Runner runner; playden::installs::InstallService service(runner, path);
      ww::steam_integration::DownloadCoordinator coordinator(service);
      assert(coordinator.Install(440)); Wait([&] { return runner.starts == 1; });
      assert(cancel ? coordinator.Cancel(440) : coordinator.Pause(440));
      Wait([&] { return runner.stopping.load(); });
      if (!cancel) {
        Wait([&] { const auto s = coordinator.Snapshot(); return s.jobs.size() == 1 && s.jobs[0].state == playden::installs::InstallJobState::kPaused; });
        assert(!coordinator.SafeToExit());
        runner.release_install = true;
        Wait([&] { return coordinator.SafeToExit(); });
        // A stale coordinator snapshot must not hide a live service resume.
        service.Resume(playden::GameId::Steam(440));
        Wait([&] { return runner.starts == 2; });
        assert(!coordinator.SafeToExit());
      } else {
        runner.release_install = true;
        Wait([&] { return runner.removing.load(); });
        Wait([&] { return coordinator.Snapshot().jobs.empty(); });
        assert(coordinator.Busy() && !coordinator.SafeToExit());
        runner.release_remove = true;
        Wait([&] { return coordinator.SafeToExit(); });
      }
    }
    std::filesystem::remove_all(path);
  }
  {
    char path[] = "/tmp/wine11-queued-removal-XXXXXX"; assert(mkdtemp(path));
    {
      Runner runner; playden::installs::InstallService service(runner, path);
      ww::steam_integration::DownloadCoordinator coordinator(service);
      assert(coordinator.Install(440)); Wait([&] { return runner.starts == 1; });
      assert(coordinator.Install(441)); Wait([&] { return coordinator.Snapshot().jobs.size() == 2; });
      assert(coordinator.Cancel(441)); Wait([&] { return coordinator.Snapshot().jobs.size() == 1; });
      assert(!coordinator.SafeToExit());
      assert(coordinator.Pause(440)); Wait([&] { return runner.stopping.load(); });
      runner.release_install = true; Wait([&] { return runner.removing.load(); });
      assert(!coordinator.SafeToExit());
      runner.release_remove = true; Wait([&] { return coordinator.SafeToExit(); });
    }
    std::filesystem::remove_all(path);
  }
}
