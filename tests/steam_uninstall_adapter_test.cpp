#include "../src/steam_integration/ui_adapter.hpp"
#include "../src/steam_integration/uninstall_flow.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
using namespace playden;
using namespace ww::steam_integration;
using namespace std::chrono_literals;
struct Session : SessionRepository {
  mutable std::mutex mutex; mutable std::optional<StoredSession> value;
  Result<std::optional<StoredSession>> Load() const override { std::lock_guard lock(mutex); return Result<std::optional<StoredSession>>::Success(value); }
  Result<void> Save(const StoredSession& s) const override { std::lock_guard lock(mutex); value = s; return Result<void>::Success(); }
  Result<void> Clear() const override { std::lock_guard lock(mutex); value.reset(); return Result<void>::Success(); }
};
struct Auth : steam::AuthBackend {
  steam::AuthResult AuthenticateQr(const std::atomic<bool>&, const steam::ChallengeCallback&) override {
    return {steam::AuthError::kNone, StoredSession{101, "Player", "refresh", "access", {}}};
  }
  steam::AuthResult Restore(const StoredSession& s, const std::atomic<bool>&) override { return {steam::AuthError::kNone, s}; }
};
struct Library : steam::LibraryBackend {
  steam::LibraryResult Fetch(const StoredSession&, const std::atomic<bool>&) override {
    steam::LibraryResult r; r.error = steam::LibraryError::kNone; GameSummary g; g.id = GameId::Steam(365670); g.title = "Blender"; r.games.push_back(g); return r;
  }
};
struct Runner : installs::InstallRunner {
  Result Install(const GameId&, const std::string&, const std::atomic<bool>&,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(installs::InstallStep)>&) override { assert(false); return {}; }
  std::string Remove(const std::string&) override { assert(false); return {}; }
};
struct Host : TitleRemovalHost {
  std::shared_ptr<homebrew::RuntimeSnapshot> state = std::make_shared<homebrew::RuntimeSnapshot>();
  int removed = 0;
  std::shared_ptr<const homebrew::RuntimeSnapshot> Snapshot() const override { return state; }
  void Remove(std::uint32_t app) override { assert(app == 365670); ++removed; }
};
struct Ops : UninstallOperations {
  bool reinstall = false; std::uint64_t account = 0;
  int recovery_calls = 0; UninstallJournal::Entry recovery_entry{};
  bool Pause() override { return true; }
  bool CancelEmpty() override { return true; }
  bool Prepare(std::uint32_t app, const std::string& title, bool again, std::uint64_t id) override {
    assert(app == 365670 && title == "FAKE78008"); reinstall = again; account = id; return true;
  }
  bool VerifyTitle(const TitleRemoval&) override { return true; }
  bool RecordTitle(const TitleRemoval&) override { return true; }
  bool Retire() override { return true; }
  bool Reconcile() override { return true; }
  bool Cleanup() override { return true; }
  UninstallFinish Finish(bool again, std::uint64_t id) override { assert(again == reinstall && id == account); return UninstallFinish::complete; }
  UninstallJournal::Entry Recovery() override { ++recovery_calls; return recovery_entry; }
  bool RecoverTitle() override { return false; }
  bool RecoverWork() override { return false; }
};
template<class F> void Wait(F f) {
  const auto end = std::chrono::steady_clock::now() + 5s;
  while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
  assert(f());
}
int main() {
  char root[] = "/tmp/vapor-uninstall-adapter-XXXXXX"; assert(mkdtemp(root));
  const auto game_path = std::filesystem::path(root) / "365670"; assert(std::filesystem::create_directory(game_path));
  { std::ofstream out(game_path / ".playden-install.json"); out << "{\"install_bytes\":1000}"; }
  { std::ofstream out(std::filesystem::path(root) / ".playden-games"); out << "365670\tinternal\t1000\n730\tinternal\t0\tpaused\n"; }
  {
    Session sessions; Auth backend; Library library_backend;
    steam::SteamService auth(backend, sessions); LibraryService library(library_backend, sessions);
    Controller controller(auth, library); UiAdapter ui(controller); controller.SignInQr();
    Wait([&] { ui.tick(); return controller.view().library == LibraryState::ready; });
#if defined(WINE11_OWNER_INSPECT_ONLY)
    {
      UiAdapter inspecting(controller); Host inspection_host;
      inspection_host.state->status = homebrew::RuntimeHostStatus::kReady; inspection_host.state->installs_known = true;
      TitleRemoval inspection_title(inspection_host); Ops inspection_ops;
      inspection_ops.recovery_entry.phase = UninstallJournal::Phase::intent;
      UninstallFlow inspection_flow(inspection_ops, inspection_title);
      assert(inspecting.attach_uninstall(&inspection_flow)); assert(inspecting.begin_download_recovery());
      for (int i = 0; i < 5; ++i) inspecting.tick();
      assert(inspection_ops.recovery_calls == 0 && inspection_host.removed == 0);
    }
#endif
    Runner runner; installs::InstallService service(runner, root); DownloadCoordinator downloads(service); assert(ui.attach_downloads(&downloads));
    ww::SteamGame game;
    Wait([&] { ui.tick(); return ui.game(0, game) && game.install == ww::SteamInstallState::installed; });
    assert(std::string(game.management_reason) == "Removal host not connected");
    Host host; host.state->status = homebrew::RuntimeHostStatus::kReady; host.state->installs_known = true;
    homebrew::RuntimeInstallView item; item.game = GameId::Steam(365670); item.runtime_number = 365670;
    item.title_id = "FAKE78008"; item.install_path = "/data/wine11/wine11-games/365670"; host.state->installs = {item};
    TitleRemoval title(host); Ops ops; UninstallFlow flow(ops, title); assert(ui.attach_uninstall(&flow));
    Wait([&] { ui.tick(); return ui.game(0, game) && game.can_reinstall; });
    assert(downloads.Snapshot().jobs.size() == 1 && downloads.Snapshot().jobs[0].state == installs::InstallJobState::kPaused);
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
    int empty_checks = 0;
    ui.arm_owner_blender_request([&] { ++empty_checks; return OwnerBlenderClaim::absent; });
    ui.tick(); ui.tick();
    assert(empty_checks == 1 && host.removed == 0);
    int claims = 0;
    host.state->installs[0].title_id = "FAKE78007";
    ui.arm_owner_blender_request([&] { ++claims; return OwnerBlenderClaim::claimed; });
    for (int i = 0; i < 3; ++i) ui.tick();
    assert(claims == 0 && host.removed == 0);
    host.state->installs[0].title_id = "FAKE78008";
    Wait([&] { ui.tick(); return host.removed == 1; });
    assert(claims == 1 && !ops.reinstall && ops.account == 101);
#else
    assert(game.can_manage_install && ui.remove_or_reinstall(365670, true));
    assert(host.removed == 1 && ops.reinstall && ops.account == 101);
#endif
    assert(!ui.remove_or_reinstall(365670, true));
    host.state->installs.clear(); ++host.state->removals_handled;
    Wait([&] { ui.tick(); return flow.state() == UninstallFlowState::complete; });
  }
  std::filesystem::remove_all(root);
}
