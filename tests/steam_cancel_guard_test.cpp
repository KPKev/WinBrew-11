#include "../src/steam_integration/downloads.hpp"
#include "../src/steam_integration/guarded_runner.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<bool> started{false}, saw_cancel{false}; std::atomic<int> removed{0};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    started = true; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    saw_cancel = true; return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { ++removed; return {}; }
};
template<class F> void Wait(F f) { const auto end = std::chrono::steady_clock::now() + 5s; while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms); assert(f()); }
int main() {
  char root[] = "/tmp/wine11-cancel-guard-XXXXXX"; assert(mkdtemp(root));
  const auto games = std::filesystem::path(root) / "wine11-games"; assert(mkdir(games.c_str(), 0700) == 0);
  const auto lock_path = games / "wine11-install.lock";
  int lock_fd = open(lock_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600); assert(lock_fd >= 0 && close(lock_fd) == 0);
  {
    auto directory = playden::platform::ManagedDirectory::Open(games, playden::platform::DirectoryPolicy::kPrivate); assert(directory);
    auto lease = playden::platform::ManagedFileLock::Acquire(*directory, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    Runner delegate; ww::steam_integration::GuardedInstallRunner guard(*directory, *lease, delegate);
    assert(!guard.Remove((games / "440").string()).empty() && delegate.removed == 0);
    const auto game = games / "440"; assert(mkdir(game.c_str(), 0700) == 0);
    const auto sentinel = game / "keep.txt";
    FILE* file = std::fopen(sentinel.c_str(), "w"); assert(file); assert(std::fputs("keep", file) >= 0 && std::fclose(file) == 0);
    {
      playden::installs::InstallService service(guard, games.string());
      ww::steam_integration::DownloadCoordinator coordinator(service);
      assert(coordinator.Install(440)); Wait([&] { return delegate.started.load(); });
      assert(!coordinator.Cancel(440));
      assert(coordinator.Error().find("Pause") != std::string::npos);
      service.Cancel(playden::GameId::Steam(440));
      std::this_thread::sleep_for(30ms);
      assert(!delegate.saw_cancel && delegate.removed == 0);
      assert(coordinator.Pause(440)); Wait([&] { return delegate.saw_cancel.load(); });
      Wait([&] { return coordinator.SafeToExit(); });
      assert(delegate.removed == 0 && std::filesystem::exists(sentinel));
    }
    // A previously persisted uninstall must not bypass the guarded runner.
    const auto record = game / ".playden-install.json";
    file = std::fopen(record.c_str(), "w"); assert(file); assert(std::fputs("{}", file) >= 0 && std::fclose(file) == 0);
    const auto registry = games / ".playden-games";
    file = std::fopen(registry.c_str(), "w"); assert(file); assert(std::fputs("440\tinternal\t100\tremoving\n", file) >= 0 && std::fclose(file) == 0);
    {
      playden::installs::InstallService service(guard, games.string());
      Wait([&] { return !service.Snapshot().installed.empty(); });
      assert(delegate.removed == 0 && std::filesystem::exists(record) && std::filesystem::exists(sentinel));
      const auto failures = service.TakeRemovalFailures(); assert(failures.size() == 1 && !failures[0].reason.empty());
      service.Uninstall(playden::GameId::Steam(440));
      assert(service.Snapshot().installed.size() == 1 && delegate.removed == 0);
    }
  }
  std::filesystem::remove_all(root);
}
