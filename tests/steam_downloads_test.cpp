// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/downloads.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<int> started{0}, removed{0}; std::atomic<bool> finish{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
                 const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>& progress,
                 const std::function<void(playden::installs::InstallStep)>& step) override {
    ++started; step(playden::installs::InstallStep::kDownloading); progress(30, 100, 30);
    while (!cancel && !finish) std::this_thread::sleep_for(2ms);
    if (cancel) return {false,true,"",0,false,""};
    return {true,false,"",100,false,""};
  }
  std::string Remove(const std::string&) override { ++removed; return {}; }
};
template<class F> void wait(F predicate) {
  for (int i=0; i<2000 && !predicate(); ++i) std::this_thread::sleep_for(2ms);
  assert(predicate());
}
int main() {
  char path[] = "/tmp/wine11-download-test-XXXXXX"; assert(mkdtemp(path));
  {
    Runner runner; playden::installs::InstallService service(runner, path);
    ww::steam_integration::DownloadCoordinator coordinator(service);
    assert(!coordinator.Install(0)); assert(coordinator.Install(440));
    wait([&]{ auto s=coordinator.Snapshot(); return !s.jobs.empty() && s.jobs[0].done_bytes==30; });
    playden::installs::InstallSnapshot presentation;
    std::uint64_t generation = 0;
    assert(coordinator.SnapshotIfChanged(presentation, generation));
    assert(generation && !presentation.jobs.empty() && presentation.jobs[0].done_bytes == 30);
    // A render-frame poll never deep-copies the same publication again.
    bool unchanged_observed = false;
    for (int i=0; i<50; ++i) {
      const auto previous = generation;
      presentation.default_volume_id = "unchanged-sentinel";
      if (!coordinator.SnapshotIfChanged(presentation, generation)) {
        assert(generation == previous && presentation.default_volume_id == "unchanged-sentinel");
        unchanged_observed = true;
      }
    }
    assert(unchanged_observed);
    assert(!coordinator.SafeToExit());
    assert(runner.started==1);
    assert(coordinator.Pause(440));
    wait([&]{ auto s=coordinator.Snapshot(); return !s.jobs.empty() && s.jobs[0].state==playden::installs::InstallJobState::kPaused; });
    wait([&]{ return coordinator.SafeToExit(); });
    assert(coordinator.SnapshotIfChanged(presentation, generation));
    assert(!presentation.jobs.empty() && presentation.jobs[0].state == playden::installs::InstallJobState::kPaused);
    assert(coordinator.Resume(440));
    wait([&]{ return runner.started >= 2; });
    assert(coordinator.Cancel(440));
    wait([&]{ return runner.removed > 0; });
    wait([&]{ return coordinator.Snapshot().jobs.empty(); });
    assert(runner.removed==1);
  }
  std::filesystem::remove_all(path);
  std::puts("PASS: Playden download worker queue, progress, pause, resume and explicit cancel");
}
