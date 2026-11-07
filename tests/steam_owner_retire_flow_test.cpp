#include "../src/steam_integration/uninstall_flow.hpp"
#include <cassert>
#include <chrono>
#include <thread>
using namespace ww::steam_integration;
using namespace playden;
using namespace playden::homebrew;
using namespace std::chrono_literals;
struct Host : TitleRemovalHost {
  std::shared_ptr<RuntimeSnapshot> snapshot = std::make_shared<RuntimeSnapshot>(); int requests = 0;
  std::shared_ptr<const RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Remove(std::uint32_t app) override { assert(app == 365670); ++requests; }
};
struct Ops : UninstallOperations {
  UninstallJournal::Entry entry; bool claim = false; int claims = 0;
  bool Pause() override { return false; } bool CancelEmpty() override { return false; }
  bool Prepare(std::uint32_t, const std::string&, bool, std::uint64_t) override { return false; }
  bool VerifyTitle(const TitleRemoval&) override { return true; } bool RecordTitle(const TitleRemoval&) override { return true; }
  bool Retire() override { return true; } bool Reconcile() override { return true; } bool Cleanup() override { return true; }
  UninstallJournal::Entry Recovery() override { return entry; } bool RecoverTitle() override { return false; }
  bool RecoverWork() override { return false; } UninstallFinish Finish(bool, std::uint64_t) override { return UninstallFinish::complete; }
  bool ClaimOwnerRetireRecovery(const UninstallJournal::Entry&) override { ++claims; return claim; }
};
int main() {
  for (int scenario = 0; scenario < 5; ++scenario) {
    Host host; host.snapshot->status = RuntimeHostStatus::kReady; host.snapshot->installs_known = true;
    RuntimeInstallView record; record.game = GameId::Steam(365670); record.runtime_number = 365670; record.title_id = "FAKE78008";
    record.install_path = "/data/wine11/wine11-games/365670"; record.state = playden::runtime::PreparationState::kRemoving;
    record.step = scenario == 2 ? "unregister" : "retire"; record.failure_code = scenario == 3 ? "unregister-refused" : "retire-failed";
    host.snapshot->installs = {record}; if (scenario == 4) host.snapshot->installs.push_back(record);
    TitleRemoval title(host); Ops ops; ops.entry.phase = UninstallJournal::Phase::intent; ops.entry.app = 365670;
    ops.entry.title = "FAKE78008"; ops.entry.operation = "11111111-2222-4333-8444-555555555555"; ops.claim = scenario != 0;
    UninstallFlow flow(ops, title); const bool started = flow.Recover();
    if (scenario == 1) {
      assert(started && ops.claims == 1 && host.requests == 1);
      host.snapshot->installs.clear(); ++host.snapshot->removals_handled;
      const auto end = std::chrono::steady_clock::now() + 5s;
      while (flow.state() != UninstallFlowState::complete && flow.state() != UninstallFlowState::failed && std::chrono::steady_clock::now() < end) {
        flow.Tick(); std::this_thread::sleep_for(1ms);
      }
      assert(flow.state() == UninstallFlowState::complete && host.requests == 1);
    } else {
      assert(host.requests == 0);
      if (scenario == 0) assert(!started && ops.claims == 1);
      else assert(ops.claims == 0);
    }
  }
}
