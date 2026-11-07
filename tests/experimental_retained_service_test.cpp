// Host-only composition of the experimental ledger and generated service.
// The restart gate deliberately never opens: this is not recovery completion.
#include "experimental_retained_ledger.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include "installs/install_service.h"
#include "platform/ps5/title_allow_list.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
using namespace std::chrono_literals;
struct Runner : playden::installs::InstallRunner {
  std::atomic<int> started{0}, removed{0};
  std::atomic<bool> canceled{false}, release{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    const auto uid = playden::ps5::HoldEffectiveUid(); ++started;
    while (!cancel.load()) std::this_thread::sleep_for(1ms);
    canceled = true;
    while (!release.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { ++removed; return {}; }
};
template<class F> void Wait(F f) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  char root[] = "/tmp/wine11-retained-service-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    ExperimentalRetainedLedger ledger(paths->state, paths->state_lease); assert(ledger.Initialize());
    auto game = paths->games.path() / "440";
    std::filesystem::create_directory(game);
    { std::ofstream data(game / "user-data"); data << "keep"; }
    Runner runner;
    std::atomic<int> saved{0};
    const std::vector<playden::installs::InstallVolume> volumes = {{"internal", "Console", paths->games.path().string(), true}};
    auto one = playden::GameId::Steam(440), two = playden::GameId::Steam(441);
    {
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, volumes, [] { return true; },
        [&](playden::GameId id) {
          auto uid = playden::ps5::TryHoldEffectiveUid(); if (!uid) return false;
          const auto app = id.steam_app();
          const bool ok = app && ledger.Retain(*app, {111, 222, 333, *app});
          if (ok) ++saved;
          return ok;
        });
      service.Install(one); Wait([&] { return runner.started == 1; });
      service.Install(two); service.Cancel(two); service.Cancel(one);
      Wait([&] { return runner.canceled.load(); });
      assert(saved == 0 && runner.removed == 0);
      runner.release = true;
      Wait([&] { return saved == 2; });
      auto read = ledger.Read(); assert(read && read->size() == 2);
      assert(service.Snapshot().jobs.size() == 2 && runner.removed == 0);
    }
    {
      playden::installs::InstallService service({{playden::Store::kSteam, &runner}}, volumes,
        [&] { auto entries = ledger.Read(); assert(entries && entries->size() == 2); return false; });
      assert(service.Snapshot().recovery_blocked && runner.started == 1 && runner.removed == 0);
    }
    std::ifstream data(game / "user-data"); std::string content; data >> content; assert(content == "keep");
  }
  close(anchor); std::filesystem::remove_all(root);
}
