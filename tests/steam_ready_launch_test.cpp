// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/ready_launch.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
using namespace playden;
using namespace playden::homebrew;
using namespace ww::steam_integration;
struct Host final : ReadyLaunchHost {
  std::shared_ptr<RuntimeSnapshot> snapshot = std::make_shared<RuntimeSnapshot>();
  int requests = 0, handoffs = 0;
  std::shared_ptr<const RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Play(std::uint32_t app) override { assert(app == 440); ++requests; }
  bool Handoff() override { ++handoffs; return true; }
};
static RuntimeSnapshot ready() {
  RuntimeSnapshot s; s.status = RuntimeHostStatus::kReady; s.kit_available = true; s.installs_known = true;
  RuntimeInstallView game; game.game = GameId::Steam(440); game.runtime_number = 440;
  game.install_path = "/data/wine11-games/440"; game.state = runtime::PreparationState::kReady; game.admission = "ok";
  s.installs.push_back(game); return s;
}
int main() {
  Host host; *host.snapshot = ready(); CloudGateState cloud = CloudGateState::blocked;
  ReadyLaunch bridge(host, [&](std::uint32_t app, std::uint64_t account, std::uint64_t generation) { assert(app == 440 && account == 42 && generation == 3); return CloudGateResult{cloud, "Cloud not ready"}; });
  assert(!bridge.Request(440, "/data/wine11-games/440", 42, 3) && host.requests == 0);
  cloud = CloudGateState::ready;
  assert(!bridge.Request(440, "/data/other/440", 42, 3) && host.requests == 0);
  host.snapshot->installs[0].recovery_pending = true;
  assert(!bridge.Request(440, "/data/wine11-games/440", 42, 3) && host.requests == 0);
  host.snapshot->installs[0].recovery_pending = false;
  bool cross_thread_result = true;
  std::thread other([&] { cross_thread_result = bridge.Request(440, "/data/wine11-games/440", 42, 3); }); other.join();
  assert(!cross_thread_result && host.requests == 0);
  assert(bridge.Request(440, "/data/wine11-games/440", 42, 3) && host.requests == 1);
  // The bridge is the host's sole Play requester; a second game cannot be queued while this one waits.
  assert(!bridge.Request(441, "/data/wine11-games/441", 42, 3) && host.requests == 1);
  bridge.Tick(42, 3); assert(host.handoffs == 0);
  host.snapshot->plays_handled = 1;
  RuntimeLaunchView launch; launch.game = GameId::Steam(441); launch.state = runtime::LaunchState::kReadyToHandoff;
  host.snapshot->launch = launch; host.snapshot->handoff_ready = true;
  bridge.Tick(42, 3); assert(host.handoffs == 0);
  host.snapshot->launch->game = GameId::Steam(440);
  bridge.Tick(41, 3); assert(host.handoffs == 0 && bridge.Busy());
  std::thread other_tick([&] { bridge.Tick(42, 3); }); other_tick.join();
  assert(host.handoffs == 0);
  cloud = CloudGateState::blocked; bridge.Tick(42, 3); assert(host.handoffs == 0 && bridge.Busy());
  cloud = CloudGateState::ready; bridge.Tick(42, 3); assert(host.handoffs == 1 && bridge.view().state == ReadyLaunchState::handoff);
  bridge.Tick(42, 3); assert(host.handoffs == 1);
  // Handoff returning true only means the host attempted it; the native outcome may still fail.
  host.snapshot->launch->state = runtime::LaunchState::kFailed; host.snapshot->launch->message = "Native refusal";
  bridge.Tick(42, 3); assert(bridge.view().state == ReadyLaunchState::failed && bridge.view().message == "Native refusal");
  *host.snapshot = ready(); host.snapshot->plays_handled = 1;
  assert(bridge.Request(440, "/data/wine11-games/440", 42, 3));
  host.snapshot->plays_handled = 2; host.snapshot->play_refusals[440] = "Ownership changed";
  bridge.Tick(42, 3); assert(bridge.view().state == ReadyLaunchState::failed && host.handoffs == 1);
  *host.snapshot = ready(); cloud = CloudGateState::checking;
  assert(bridge.Request(440, "/data/wine11-games/440", 42, 3) && host.requests == 2 && bridge.Busy());
  bridge.Tick(42, 3); assert(host.requests == 2);
  host.snapshot->installs[0].install_path = "/data/changed/440";
  cloud = CloudGateState::ready; bridge.Tick(42, 3);
  assert(bridge.view().state == ReadyLaunchState::failed && host.requests == 2);
  *host.snapshot = ready(); cloud = CloudGateState::checking;
  assert(bridge.Request(440, "/data/wine11-games/440", 42, 3));
  bridge.Tick(41, 3); assert(bridge.view().state == ReadyLaunchState::failed && host.requests == 2);
  assert(bridge.Request(440, "/data/wine11-games/440", 42, 3));
  cloud = CloudGateState::ready; bridge.Tick(42, 3);
  assert(host.requests == 3 && bridge.Busy());
  std::puts("PASS: prepared identity, cloud gate, thread affinity, unrelated sessions and native failure");
}
