#include "../src/steam_integration/uninstall_flow.hpp"
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
using namespace ww::steam_integration;
using namespace playden;
using namespace playden::homebrew;
using namespace std::chrono_literals;
struct Host : TitleRemovalHost {
  std::shared_ptr<RuntimeSnapshot> snapshot = std::make_shared<RuntimeSnapshot>();
  std::shared_ptr<const RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Remove(std::uint32_t app) override { assert(app == 365670); }
};
struct Ops : UninstallOperations {
  std::mutex mutex; std::vector<int> calls; int fail = 0;
  UninstallJournal::Entry recovery;
  bool step(int n) { std::lock_guard<std::mutex> lock(mutex); calls.push_back(n); return fail != n; }
  bool Pause() override { return step(1); }
  bool CancelEmpty() override { return true; }
  bool Prepare(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account) override {
    assert(app == 365670 && title == "FAKE78008" && reinstall && account == 101); return step(2);
  }
  bool VerifyTitle(const TitleRemoval& title) override { assert(title.state() == TitleRemovalState::removed); return step(3); }
  bool RecordTitle(const TitleRemoval&) override { return step(4); }
  bool Retire() override { return step(5); }
  bool Reconcile() override { return step(6); }
  bool Cleanup() override { return step(7); }
  UninstallJournal::Entry Recovery() override { return recovery; }
  bool RecoverTitle() override { return step(9); }
  bool RecoverWork() override { return step(10); }
  UninstallFinish Finish(bool reinstall, std::uint64_t account) override {
    assert(reinstall && account == 101); return step(8) ? UninstallFinish::complete : UninstallFinish::failed;
  }
};
int main() {
  for (int fail : {0, 3, 4, 5, 6, 7, 8}) {
    Host host; host.snapshot->status = RuntimeHostStatus::kReady; host.snapshot->installs_known = true;
    RuntimeInstallView record; record.game = GameId::Steam(365670); record.runtime_number = 365670; record.title_id = "FAKE78008";
    record.install_path = "/data/wine11/wine11-games/365670";
    host.snapshot->installs = {record}; TitleRemoval title(host); Ops ops; ops.fail = fail;
    UninstallFlow flow(ops, title); assert(flow.Begin(365670, "FAKE78008", true, 101));
    flow.Tick(); assert(flow.state() == UninstallFlowState::title); // not yet confirmed
    host.snapshot->installs.clear(); ++host.snapshot->removals_handled;
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (flow.state() != UninstallFlowState::complete && flow.state() != UninstallFlowState::failed && std::chrono::steady_clock::now() < end) {
      flow.Tick(); std::this_thread::sleep_for(1ms);
    }
    assert(flow.state() == (fail ? UninstallFlowState::failed : UninstallFlowState::complete));
    assert(!ops.calls.empty() && ops.calls.back() == (fail ? fail : 8));
    for (std::size_t i = 1; i < ops.calls.size(); ++i) assert(ops.calls[i] == ops.calls[i - 1] + 1);
  }
  {
    Host host; host.snapshot->status = RuntimeHostStatus::kReady; host.snapshot->installs_known = true;
    TitleRemoval title(host); Ops ops;
    ops.recovery.phase = UninstallJournal::Phase::intent; ops.recovery.app = 365670; ops.recovery.title = "FAKE78008";
    ops.recovery.operation = "11111111-2222-4333-8444-555555555555"; ops.recovery.reinstall = true; ops.recovery.account = 101;
    UninstallFlow flow(ops, title); assert(flow.Recover());
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (flow.state() != UninstallFlowState::complete && flow.state() != UninstallFlowState::failed && std::chrono::steady_clock::now() < end) {
      flow.Tick(); std::this_thread::sleep_for(1ms);
    }
    assert(flow.state() == UninstallFlowState::complete);
    assert((ops.calls == std::vector<int>{9, 10, 8}));
  }
}
