#include "installs/install_service.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<int> starts{0}, removals{0};
  std::atomic<bool> stop_seen{false}, release_install{false}, release_remove{false}, fail_remove{true}, throw_remove{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    ++starts; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    stop_seen = true; while (!release_install.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override {
    ++removals; while (!release_remove.load()) std::this_thread::sleep_for(1ms);
    if (throw_remove.load()) throw 1;
    return fail_remove.load() ? "Synthetic cleanup error." : std::string();
  }
};
template<class F> void Wait(F f) { const auto end=std::chrono::steady_clock::now()+5s; while (!f() && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(1ms); assert(f()); }
int main() {
  for (bool queued : {false, true}) {
    char path[] = "/tmp/wine11-cleanup-failure-XXXXXX"; assert(mkdtemp(path));
    {
      Runner runner; playden::installs::InstallService service(runner, path);
      const auto first = playden::GameId::Steam(440), canceled = playden::GameId::Steam(queued ? 441 : 440);
      service.Install(first); Wait([&] { return runner.starts == 1; });
      if (queued) service.Install(canceled);
      service.Cancel(canceled);
      if (queued) service.Pause(first);
      Wait([&] { return runner.stop_seen.load(); }); runner.release_install = true;
      Wait([&] { return runner.removals == 1; });
      auto before = service.Snapshot();
      bool found = false; for (const auto& job : before.jobs) if (job.id == canceled) found = true;
      assert(found && !service.WorkerIdleForExit());
      service.Install(canceled); service.Resume(canceled); service.Cancel(canceled);
      assert(runner.starts == 1 && runner.removals == 1);
      runner.release_remove = true;
      Wait([&] { for (const auto& job : service.Snapshot().jobs) if (job.id == canceled && job.state == playden::installs::InstallJobState::kFailed && job.failure.find("Synthetic cleanup error") != std::string::npos) return true; return false; });
      service.Resume(canceled);
      service.Pause(canceled);
      { bool blocked = false; for (const auto& job : service.Snapshot().jobs) if (job.id == canceled) blocked = job.cleanup_failed && job.cleanup_retryable && job.state == playden::installs::InstallJobState::kFailed; assert(blocked); }
      runner.fail_remove = false; runner.throw_remove = true; service.Cancel(canceled);
      Wait([&] { for (const auto& job : service.Snapshot().jobs) if (job.id == canceled && job.cleanup_failed && job.failure.find("unexpectedly") != std::string::npos) return true; return false; });
      runner.throw_remove = false; service.Cancel(canceled);
      Wait([&] { for (const auto& job : service.Snapshot().jobs) if (job.id == canceled) return false; return runner.removals == 3; });
      assert(runner.starts == 1);
    }
    std::filesystem::remove_all(path);
  }
  char root[] = "/tmp/wine11-cleanup-volume-XXXXXX"; assert(mkdtemp(root));
  {
    Runner runner;
    std::vector<playden::installs::InstallVolume> volumes = {{"internal", "Internal", root, true}, {"external", "External", std::string(root) + "/missing", false}};
    playden::installs::InstallService service(runner, volumes);
    const auto id = playden::GameId::Steam(442); service.Install(id, std::nullopt, "external"); service.Cancel(id);
    const auto snapshot = service.Snapshot();
    assert(snapshot.jobs.size() == 1 && snapshot.jobs[0].state == playden::installs::InstallJobState::kFailed);
    assert(snapshot.jobs[0].failure.find("Storage unavailable") != std::string::npos && runner.removals == 0);
  }
  std::filesystem::remove_all(root);
  for (bool record_present : {false, true}) {
    char path[] = "/tmp/wine11-cleanup-restart-XXXXXX"; assert(mkdtemp(path));
    const auto game = std::string(path) + "/440"; assert(std::filesystem::create_directory(game));
    {
      Runner runner; runner.release_install = true; runner.release_remove = true;
      playden::installs::InstallService service(runner, path);
      service.Install(playden::GameId::Steam(440)); Wait([&] { return runner.starts == 1; });
      service.Cancel(playden::GameId::Steam(440));
      Wait([&] { const auto s = service.Snapshot(); return s.jobs.size() == 1 && s.jobs[0].cleanup_failed; });
    }
    if (record_present) {
      const auto record = game + "/.playden-install.json";
      FILE* file = std::fopen(record.c_str(), "w"); assert(file); assert(std::fputs("{}", file) >= 0 && std::fclose(file) == 0);
    }
    {
      Runner runner; runner.release_remove = true; runner.fail_remove = false;
      playden::installs::InstallService service(runner, path);
      const auto snapshot = service.Snapshot();
      assert(snapshot.jobs.size() == 1 && snapshot.jobs[0].cleanup_failed && snapshot.jobs[0].cleanup_retryable == !record_present);
      service.Resume(playden::GameId::Steam(440));
      assert(service.Snapshot().jobs[0].state == playden::installs::InstallJobState::kFailed && runner.starts == 0);
      service.Cancel(playden::GameId::Steam(440));
      if (!record_present) Wait([&] { return service.Snapshot().jobs.empty() && runner.removals == 1; });
      else assert(service.Snapshot().jobs.size() == 1 && runner.removals == 0);
    }
    std::filesystem::remove_all(path);
  }
}
