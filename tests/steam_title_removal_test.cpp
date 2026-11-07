#include "../src/steam_integration/title_removal.hpp"
#include <cassert>
using namespace playden;
using namespace playden::homebrew;
using namespace ww::steam_integration;
struct Fake : TitleRemovalHost {
  std::shared_ptr<RuntimeSnapshot> snapshot = std::make_shared<RuntimeSnapshot>();
  int requests = 0; std::uint32_t requested = 0;
  std::shared_ptr<const RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Remove(std::uint32_t app) override { ++requests; requested = app; }
};
int main() {
  Fake host; host.snapshot->status = RuntimeHostStatus::kReady; host.snapshot->installs_known = true;
  RuntimeInstallView record; record.runtime_number = 365670; record.game = GameId::Steam(365670); record.title_id = "FAKE78008";
  record.install_path = "/data/wine11/wine11-games/365670";
  host.snapshot->installs.push_back(record); TitleRemoval removal(host);
  host.snapshot->installs[0].state = playden::runtime::PreparationState::kRemoving;
  host.snapshot->installs[0].step = "unregister";
  assert(!removal.Begin(365670, "FAKE78008") && host.requests == 0);
  host.snapshot->installs[0] = record;
  assert(!removal.Begin(365670, "FAKE78007")); assert(host.requests == 0);
  assert(removal.Begin(365670, "FAKE78008") && host.requests == 1);
  assert(!removal.Begin(365670, "FAKE78008"));
  ++host.snapshot->removals_handled; removal.Tick(); assert(removal.state() == TitleRemovalState::waiting);
  host.snapshot->installs_known = false; host.snapshot->installs.clear(); removal.Tick(); assert(removal.state() == TitleRemovalState::waiting);
  host.snapshot->installs_known = true; removal.Tick(); assert(removal.state() == TitleRemovalState::removed);
  host.snapshot->installs.push_back(record); assert(removal.Begin(365670, "FAKE78008"));
  ++host.snapshot->removals_handled; host.snapshot->removal_failures[365670] = "unregister refused";
  removal.Tick(); assert(removal.state() == TitleRemovalState::failed && removal.message() == "unregister refused");
}
