#include "../src/steam_integration/ui_adapter.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
using namespace std::chrono_literals;
using namespace playden;
struct Session : SessionRepository {
  mutable std::mutex mutex; mutable std::optional<StoredSession> value;
  Result<std::optional<StoredSession>> Load() const override { std::lock_guard<std::mutex> lock(mutex); return Result<std::optional<StoredSession>>::Success(value); }
  Result<void> Save(const StoredSession& session) const override { std::lock_guard<std::mutex> lock(mutex); value = session; return Result<void>::Success(); }
  Result<void> Clear() const override { std::lock_guard<std::mutex> lock(mutex); value.reset(); return Result<void>::Success(); }
};
struct Auth : steam::AuthBackend {
  steam::AuthResult AuthenticateQr(const std::atomic<bool>&, const steam::ChallengeCallback&) override {
    return {steam::AuthError::kNone, StoredSession{42, "Player", "refresh", "access", {}}};
  }
  steam::AuthResult Restore(const StoredSession& session, const std::atomic<bool>&) override { return {steam::AuthError::kNone, session}; }
};
struct Library : steam::LibraryBackend {
  steam::LibraryResult Fetch(const StoredSession&, const std::atomic<bool>&) override {
    steam::LibraryResult result; result.error = steam::LibraryError::kNone;
    GameSummary game; game.id = GameId::Steam(440); game.title = "Game"; result.games.push_back(game); return result;
  }
};
struct Host : ww::steam_integration::PreparationHost {
  std::shared_ptr<homebrew::RuntimeSnapshot> state = std::make_shared<homebrew::RuntimeSnapshot>();
  std::shared_ptr<const homebrew::RuntimeSnapshot> Snapshot() const override { return state; }
  void Prepare(homebrew::RuntimePrepareRequest) override { assert(false); }
};
struct Runner : installs::InstallRunner {
  std::atomic<bool> started{false}, stopping{false}, release{false};
  Result Install(const GameId&, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(installs::InstallStep)>&) override {
    started = true; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    stopping = true; while (!release.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { return {}; }
};
template<class F> void Wait(F f) { const auto end = std::chrono::steady_clock::now() + 5s; while (!f() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms); assert(f()); }
int main() {
  char root[] = "/tmp/wine11-idle-adapter-XXXXXX"; assert(mkdtemp(root));
  {
    Session sessions; Auth auth_backend; Library backend;
    steam::SteamService auth(auth_backend, sessions); LibraryService library(backend, sessions);
    ww::steam_integration::Controller controller(auth, library); ww::steam_integration::UiAdapter ui(controller);
    controller.SignInQr(); Wait([&] { ui.tick(); return controller.view().library == ww::steam_integration::LibraryState::ready; });
    Host host; host.state->installs_known = true;
    homebrew::RuntimeInstallView game; game.game = GameId::Steam(440); game.runtime_number = 440; game.awaiting_registration = true;
    host.state->installs.push_back(game);
    ww::steam_integration::Preparation preparation(host); assert(ui.attach_preparation(&preparation));
    Runner runner; installs::InstallService service(runner, root); ww::steam_integration::DownloadCoordinator downloads(service);
    assert(ui.begin_download_recovery());
    assert(!ui.attach_downloads(&downloads));
    assert(!ui.finish_setup(440));
    assert(ui.finish_download_maintenance(&downloads));
    Wait([&] { return downloads.SafeToExit(); });
    assert(!ui.detach_downloads_for_maintenance());
    assert(downloads.FreezeForMaintenance());
    assert(ui.detach_downloads_for_maintenance());
    assert(!ui.finish_download_maintenance(&downloads)); // still frozen
    ui.sign_out(); assert(controller.view().account == ww::steam_integration::AccountState::online);
    assert(!ui.finish_setup(440));
    assert(downloads.ThawAfterMaintenance());
    assert(ui.finish_download_maintenance(&downloads));
    assert(downloads.Install(441)); Wait([&] { return runner.started.load(); });
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
    Wait([&] { ui.tick(); ww::SteamDownload job; return ui.download(0, job) && job.app_id == 441; });
    assert(!ui.can_cancel_download() && !ui.cancel_download(441));
    assert(downloads.Error().find("Pause") != std::string::npos && !runner.stopping.load());
#endif
    assert(downloads.Pause(441)); Wait([&] { return runner.stopping.load(); });
    Wait([&] { const auto s = downloads.Snapshot(); return s.jobs.size() == 1 && s.jobs[0].state == installs::InstallJobState::kPaused; });
    assert(!ui.finish_setup(440));
    runner.release = true; Wait([&] { return downloads.SafeToExit(); });
    assert(ui.finish_setup(440));
  }
  std::filesystem::remove_all(root);
}
