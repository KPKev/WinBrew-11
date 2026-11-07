// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/cloud_preflight.hpp"
#include "../src/steam_integration/cloud_launch_gate.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>
using namespace playden;
using namespace playden::steam;
using namespace ww::steam_integration;
struct Sessions : SessionRepository {
  Result<std::optional<StoredSession>> Load() const override { return Result<std::optional<StoredSession>>::Success(StoredSession{42,"Player","r","a",{}}); }
  Result<void> Save(const StoredSession&) const override { return Result<void>::Success(); }
  Result<void> Clear() const override { return Result<void>::Success(); }
};
struct Runner : CloudRunner {
  std::atomic<bool> hold{false}, entered{false}; std::atomic<CloudState> answer{CloudState::kUpToDate};
  std::atomic<int> applies{0}; std::atomic<CloudResolution> last_choice{CloudResolution::kAutomatic};
  CloudReport Check(const StoredSession&, const CloudLocation&, const std::atomic<bool>& cancel) override {
    entered = true; while (hold && !cancel) std::this_thread::yield();
    CloudReport result; result.state = answer.load(); result.message = "test result"; return result;
  }
  CloudReport Apply(const StoredSession&, const CloudLocation&, CloudResolution choice, bool, const std::atomic<bool>&) override {
    last_choice = choice; ++applies; CloudReport result; result.state = CloudState::kSynced; return result;
  }
};
struct Host : ReadyLaunchHost {
  std::shared_ptr<homebrew::RuntimeSnapshot> snapshot = std::make_shared<homebrew::RuntimeSnapshot>();
  int requests = 0;
  std::shared_ptr<const homebrew::RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Play(std::uint32_t app) override { assert(app == 440); ++requests; }
  bool Handoff() override { return true; }
};
int main() {
  char path[] = "/tmp/wine11-cloud-test-XXXXXX"; assert(mkdtemp(path));
  Sessions sessions; Runner runner; CloudService service(runner, sessions); CloudPreflight gate(service);
  CloudLocation location; location.id = GameId::Steam(440); location.game_root = path;
  location.state_root = path; location.prefix_root = path;
  runner.hold = true; assert(gate.Begin(location, 42, 3));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!runner.entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(runner.entered && gate.Poll(440, 42, 3).state == CloudGateState::checking);
  gate.Observe(43, 3);
  assert(gate.Poll(440, 43, 3).state == CloudGateState::blocked && gate.Busy());
  runner.hold = false;
  while (gate.Busy() && std::chrono::steady_clock::now() < deadline) { gate.Observe(43, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::blocked);
  assert(!gate.Resolve(440, 42, 3, CloudResolution::kUseCloud, false));
  runner.entered = false; assert(gate.Begin(location, 42, 3));
  const auto deadline2 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < deadline2) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::ready);
  // A completed check must not carry over to a later account session.
  assert(gate.Poll(440, 42, 4).state == CloudGateState::blocked);
  assert(gate.Poll(440, 42, 3).state == CloudGateState::blocked);
  runner.answer = CloudState::kConflict; assert(gate.Begin(location, 42, 3));
  const auto deadline3 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < deadline3) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::blocked);
  assert(gate.ChoiceApp(42, 3) == 440 && gate.ChoiceApp(43, 3) == 0);
  assert(!gate.Resolve(440, 43, 3, CloudResolution::kUseLocal, false));
  assert(!gate.Resolve(440, 42, 3, CloudResolution::kAutomatic, false));
  assert(gate.Resolve(440, 42, 3, CloudResolution::kUseCloud, false));
  const auto resolve_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < resolve_deadline) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::ready);
  assert(runner.applies == 1 && runner.last_choice == CloudResolution::kUseCloud);
  runner.answer = CloudState::kNeedsAccount; assert(gate.Begin(location, 42, 3));
  const auto account_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < account_deadline) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(gate.ChoiceApp(42, 3) == 440);
  assert(!gate.Resolve(440, 42, 3, CloudResolution::kUseLocal, false));
  assert(gate.Resolve(440, 42, 3, CloudResolution::kUseLocal, true));
  const auto account_resolve_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < account_resolve_deadline) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::ready);
  assert(runner.applies == 2 && runner.last_choice == CloudResolution::kUseLocal);
  runner.answer = CloudState::kUnsupported; assert(gate.Begin(location, 42, 3));
  const auto deadline4 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (gate.Busy() && std::chrono::steady_clock::now() < deadline4) { gate.Poll(440, 42, 3); std::this_thread::yield(); }
  assert(!gate.Busy() && gate.Poll(440, 42, 3).state == CloudGateState::blocked);
  assert(gate.SkipApp(42, 3) == 440 && gate.SkipApp(43, 3) == 0);
  assert(!gate.AllowWithoutCloud(440, 43, 3));
  assert(gate.AllowWithoutCloud(440, 42, 3));
  assert(gate.Poll(440, 42, 3).state == CloudGateState::ready && !gate.AllowWithoutCloud(440, 42, 3));
  Host host; host.snapshot->status = homebrew::RuntimeHostStatus::kReady; host.snapshot->kit_available = true;
  homebrew::RuntimeInstallView installed; installed.game = location.id; installed.runtime_number = 440;
  installed.install_path = location.game_root; installed.state = runtime::PreparationState::kReady; installed.admission = "ok";
  host.snapshot->installs.push_back(installed);
  CloudLaunchGate launch_gate(gate, [&](std::uint32_t app) -> std::optional<CloudLocation> { assert(app == 440); return location; });
  std::string reason;
  assert(!launch_gate.Begin(440, "/wrong/game", 42, 3, &reason) && !gate.Busy());
  ReadyLaunch launch(host,
    [&](std::uint32_t app, std::uint64_t account, std::uint64_t generation) { return launch_gate.Poll(app, account, generation); },
    [&](std::uint32_t app, const std::string& root, std::uint64_t account, std::uint64_t generation, std::string* error) {
      return launch_gate.Begin(app, root, account, generation, error);
    });
  assert(launch.RequestUsingCompletedCloud(440, path, 42, 3));
  assert(host.requests == 1);
  host.snapshot->plays_handled = 1;
  homebrew::RuntimeLaunchView failed; failed.game = GameId::Steam(440); failed.state = runtime::LaunchState::kFailed;
  host.snapshot->launch = failed;
  launch.Tick(42, 3); assert(!launch.Busy());
  host.requests = 0; host.snapshot->launch.reset(); host.snapshot->plays_handled = 0;
  runner.answer = CloudState::kUpToDate; runner.entered = false; runner.hold = true;
  assert(launch.Request(440, path, 42, 3) && host.requests == 0);
  const auto deadline5 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!runner.entered && std::chrono::steady_clock::now() < deadline5) std::this_thread::yield();
  assert(runner.entered); launch.Tick(42, 3); assert(host.requests == 0);
  runner.hold = false;
  while (!host.requests && std::chrono::steady_clock::now() < deadline5) { launch.Tick(42, 3); std::this_thread::yield(); }
  assert(host.requests == 1);
  service.Shutdown(); std::filesystem::remove_all(path);
  std::puts("PASS: Cloud preflight waits for a fresh result and blocks conflicts or account changes");
}
