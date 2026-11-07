// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/preparation.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
using namespace playden;
using namespace playden::homebrew;
using namespace ww::steam_integration;
struct Host : PreparationHost {
  std::shared_ptr<RuntimeSnapshot> snapshot = std::make_shared<RuntimeSnapshot>();
  int calls = 0;
  std::uint32_t expected_app = 440;
  bool fast_refusal = false;
  std::shared_ptr<const RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Prepare(RuntimePrepareRequest r) override {
    assert(r.runtime_number == expected_app); ++calls;
    if (fast_refusal) { ++snapshot->prepares_handled; snapshot->refusals[440] = "No launch option"; }
    else snapshot->queued_prepares.push_back(expected_app);
  }
};
int main() {
  Host host; host.snapshot->status = RuntimeHostStatus::kReady; host.snapshot->kit_available = true;
  Preparation prep(host);
  RuntimePrepareRequest request; request.runtime_number = 440; request.game = GameId::Steam(440);
  request.install_path = "/private/wine11-games/440"; request.volume_id = "internal"; request.title_name = "Game";
  auto invalid = request; invalid.game = GameId::Steam(441);
  assert(!prep.Request(invalid, 42, 3) && host.calls == 0);
  bool wrong_thread = true; std::thread t([&] { wrong_thread = prep.Request(request, 42, 3); }); t.join();
  assert(!wrong_thread && host.calls == 0);
  assert(prep.Request(request, 42, 3) && prep.Busy() && host.calls == 1);
  prep.Tick(43, 4); assert(prep.Busy());
  host.snapshot->queued_prepares.clear(); ++host.snapshot->prepares_handled;
  RuntimeInstallView installed; installed.game = GameId::Steam(440); installed.runtime_number = 440;
  installed.install_path = request.install_path; installed.state = runtime::PreparationState::kReady; installed.admission = "ok";
  host.snapshot->installs.push_back(installed);
  prep.Tick(43, 4); assert(prep.view().state == PreparationState::failed);
  assert(prep.Request(request, 42, 3) && prep.view().state == PreparationState::ready && host.calls == 1);
  host.snapshot->installs.clear();
  assert(prep.Request(request, 42, 3));
  host.snapshot->queued_prepares.clear(); ++host.snapshot->prepares_handled;
  host.snapshot->refusals[440] = "No launch option"; prep.Tick(42, 3);
  assert(prep.view().state == PreparationState::failed && prep.view().message == "No launch option");
  host.snapshot->refusals.clear();
  assert(prep.Request(request, 42, 3));
  host.snapshot->queued_prepares.clear();
  installed.state = runtime::PreparationState::kRegistering;
  installed.awaiting_registration = true; installed.recovery_pending = true;
  host.snapshot->installs = {installed};
  prep.Tick(42, 3);
  assert(!prep.Busy() && prep.view().state == PreparationState::awaiting_registration);
  assert(prep.NeedsRegistration(440) && !prep.NeedsRegistration(441));
  assert(!prep.Request(request, 42, 3));
  assert(prep.view().message.find("reopen Vapor and press Play") != std::string::npos);
  // Startup's completed registration snapshot must clear Finish Setup and
  // allow the same installed game to proceed to ReadyLaunch.
  host.snapshot->installs[0].state = runtime::PreparationState::kReady;
  host.snapshot->installs[0].awaiting_registration = false;
  host.snapshot->installs[0].recovery_pending = false;
  assert(!prep.NeedsRegistration(440));
  assert(prep.Request(request, 42, 3) && prep.view().state == PreparationState::ready);
  host.snapshot->installs.clear();
  // The same failure may complete between submission and the first poll.
  host.fast_refusal = true;
  assert(prep.Request(request, 42, 3));
  assert(prep.view().state == PreparationState::failed && prep.view().message == "No launch option");
  // Another game's completion must not end this request while it is queued.
  host.fast_refusal = false;
  assert(prep.Request(request, 42, 3));
  ++host.snapshot->prepares_handled; prep.Tick(42, 3); assert(prep.Busy());
  host.snapshot->queued_prepares.clear(); ++host.snapshot->prepares_handled;
  prep.Tick(42, 3);
  assert(prep.view().state == PreparationState::failed && prep.view().message == "No launch option");
  // A matching interrupted preparation resumes its saved journal through the
  // host. A removal, a different root, or a different runtime ID never does.
  Host recovery_host; recovery_host.snapshot->status = RuntimeHostStatus::kReady;
  recovery_host.snapshot->installs_known = true; recovery_host.snapshot->kit_available = true;
  RuntimeInstallView pending; pending.game = GameId::Steam(440); pending.runtime_number = 440;
  pending.install_path = request.install_path; pending.state = runtime::PreparationState::kFailed;
  pending.recovery_pending = true; pending.step = "publish";
  recovery_host.snapshot->installs = {pending}; Preparation recovery(recovery_host);
  assert(recovery.Request(request, 42, 3) && recovery.Busy() && recovery_host.calls == 1);
  recovery_host.snapshot->queued_prepares.clear(); ++recovery_host.snapshot->prepares_handled;
  recovery_host.snapshot->refusals[440] = "Saved setup still needs attention";
  recovery.Tick(42, 3); assert(recovery.view().state == PreparationState::failed);
  recovery_host.snapshot->installs[0].state = runtime::PreparationState::kRemoving;
  assert(!recovery.Request(request, 42, 3) && recovery_host.calls == 1);
  recovery_host.snapshot->installs[0] = pending;
  recovery_host.snapshot->installs[0].install_path = "/elsewhere";
  assert(!recovery.Request(request, 42, 3) && recovery_host.calls == 1);
  recovery_host.snapshot->installs[0] = pending;
  recovery_host.snapshot->installs[0].runtime_number = 441;
  assert(!recovery.Request(request, 42, 3) && recovery_host.calls == 1);
  recovery_host.snapshot->installs[0] = pending;
  recovery_host.snapshot->installs[0].state = runtime::PreparationState::kReady;
  recovery_host.snapshot->installs[0].admission = "ok";
  assert(recovery.Request(request, 42, 3) && recovery_host.calls == 2);
  // Older CS2/Blender Ready records can remain valid while their newly
  // selected runtime still needs refreshing. It must reach the real host.
  for (const auto app : {730u, 365670u}) {
    Host cs2; cs2.expected_app = app;
    cs2.snapshot->status = RuntimeHostStatus::kReady;
    cs2.snapshot->kit_available = true; cs2.snapshot->installs_known = true;
    auto cs2_request = request; cs2_request.runtime_number = app; cs2_request.game = GameId::Steam(app);
    auto legacy = installed; legacy.game = cs2_request.game; legacy.runtime_number = app;
    legacy.state = runtime::PreparationState::kReady; legacy.admission = "ok";
    legacy.awaiting_registration = false; legacy.recovery_pending = false; legacy.runtime_current = false;
    cs2.snapshot->installs = {legacy}; Preparation migration(cs2);
    assert(migration.Request(cs2_request, 42, 3) && migration.Busy() && cs2.calls == 1);
    cs2.snapshot->queued_prepares.clear(); ++cs2.snapshot->prepares_handled;
    migration.Tick(42, 3); assert(migration.Busy()); // stale Ready must not advance to launch
    cs2.snapshot->installs[0].runtime_current = true;
    migration.Tick(42, 3); assert(migration.view().state == PreparationState::ready);
    assert(migration.Request(cs2_request, 42, 3) && migration.view().state == PreparationState::ready && cs2.calls == 1);
  }
  std::puts("PASS: game preparation tracks identity, completion, refusal and account changes");
}
