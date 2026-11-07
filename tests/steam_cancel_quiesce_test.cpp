#include "installs/install_service.h"
#include "platform/ps5/title_allow_list.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<bool> entered{false}, canceled{false}, release{false};
  std::atomic<int> removed{0}; bool finish_success = false;
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t, std::uint64_t, std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    const auto uid = playden::ps5::HoldEffectiveUid();
    entered = true;
    while (!cancel.load()) std::this_thread::sleep_for(1ms);
    canceled = true;
    while (!release.load()) std::this_thread::sleep_for(1ms);
    return finish_success ? Result{true, false, "", 100, false, ""} : Result{false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { ++removed; return {}; }
};
template<class F> void Wait(F f) {
  const auto end = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  for (bool success : {false, true}) {
    char path[] = "/tmp/wine11-quiesce-XXXXXX"; assert(mkdtemp(path));
    {
      Runner runner; runner.finish_success = success;
      std::atomic<int> prepared{0};
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}},
          {{"internal", "Console", path, true}}, [] { return true; }, [&](playden::GameId) {
            const auto uid = playden::ps5::TryHoldEffectiveUid();
            if (!uid) return false;
            ++prepared; return true;
          });
      const auto first = playden::GameId::Steam(440), second = playden::GameId::Steam(441);
      service.Install(first); Wait([&] { return runner.entered.load(); });
      service.Install(second); service.Cancel(second);
      auto queued = service.Snapshot();
      assert(queued.jobs.size() == 2 && queued.cancel_cleanup.size() == 1);
      service.Resume(second);
      queued = service.Snapshot();
      assert(queued.jobs[1].state == playden::installs::InstallJobState::kPaused);
      service.Cancel(first); Wait([&] { return runner.canceled.load(); });
      assert(service.Snapshot().jobs.size() == 2 && runner.removed == 0 && prepared == 0);
      runner.release = true;
      if (success) {
        Wait([&] { return !service.Snapshot().installed.empty(); });
        const auto snapshot = service.Snapshot();
        assert(snapshot.installed[0].id == first && snapshot.jobs.size() == 1 && snapshot.cancel_cleanup.size() == 1);
      } else {
        Wait([&] { const auto s = service.Snapshot(); return s.jobs[0].state == playden::installs::InstallJobState::kPaused; });
        const auto snapshot = service.Snapshot();
        assert(snapshot.jobs.size() == 2 && snapshot.cancel_cleanup.size() == 2);
      }
      Wait([&] { return prepared == (success ? 1 : 2); });
      assert(runner.removed == 0);
    }
    std::filesystem::remove_all(path);
  }
}
