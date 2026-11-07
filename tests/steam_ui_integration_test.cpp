// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include "../src/steam_integration/ui_adapter.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
using namespace playden;
struct Session : SessionRepository {
  mutable std::mutex lock; mutable std::optional<StoredSession> value;
  Result<std::optional<StoredSession>> Load() const override { std::lock_guard<std::mutex> g(lock); return Result<std::optional<StoredSession>>::Success(value); }
  Result<void> Save(const StoredSession& v) const override { std::lock_guard<std::mutex> g(lock); value=v; return Result<void>::Success(); }
  Result<void> Clear() const override { std::lock_guard<std::mutex> g(lock); value.reset(); return Result<void>::Success(); }
};
struct Auth : steam::AuthBackend {
  std::atomic<bool> approved{false}, fail_once{false};
  steam::AuthResult AuthenticateQr(const std::atomic<bool>& cancel, const steam::ChallengeCallback& cb) override {
    if (fail_once.exchange(false)) return {steam::AuthError::kNetwork, {}};
    cb("https://example.test/synthetic-qr");
    while (!approved && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (cancel) return {steam::AuthError::kCanceled, {}};
    return {steam::AuthError::kNone, StoredSession{42, "Player", "refresh", "access", {}}};
  }
  steam::AuthResult Restore(const StoredSession& s, const std::atomic<bool>&) override { return {steam::AuthError::kNone, s}; }
};
struct Library : steam::LibraryBackend {
  steam::LibraryResult Fetch(const StoredSession&, const std::atomic<bool>&) override {
    steam::LibraryResult result; result.error = steam::LibraryError::kNone;
    for (unsigned i=0; i<10; ++i) { GameSummary g; g.id=GameId::Steam(440+i); g.title="Synthetic game " + std::to_string(i); g.playtime_minutes=125; g.last_played_unix_seconds=1700000000; result.games.push_back(g); }
    return result;
  }
};
struct DetailsSource : steam::AppDetailsSource {
  std::atomic<bool> hold{false}, entered{false}, canceled{false}, unavailable{false};
  std::atomic<int> calls{0};
  std::vector<steam::AppDetails> Fetch(const StoredSession&, const std::vector<GameId>& ids,
                                       const std::atomic<bool>& cancel) override {
    entered = true; ++calls;
    while (hold && !cancel) std::this_thread::yield();
    if (cancel) { canceled = true; return {}; }
    steam::AppDetails result; result.id=ids.at(0); result.available=!unavailable;
    if (unavailable) { result.reason="Temporarily unavailable"; return {result}; }
    result.install_bytes = 123456789; result.download_bytes = 98765432;
    result.launches.push_back({"game.exe", "-play", "bin", "Play"}); return {result};
  }
};
struct CloudRunner : steam::CloudRunner {
  std::atomic<bool> hold{true}, entered{false};
  std::atomic<steam::CloudState> answer{steam::CloudState::kUpToDate};
  std::atomic<int> resolved{0};
  steam::CloudReport Check(const StoredSession&, const steam::CloudLocation&, const std::atomic<bool>& cancel) override {
    entered = true; while (hold && !cancel) std::this_thread::yield();
    steam::CloudReport result; result.state = answer; return result;
  }
  steam::CloudReport Apply(const StoredSession&, const steam::CloudLocation&, steam::CloudResolution choice, bool, const std::atomic<bool>&) override {
    assert(choice == steam::CloudResolution::kUseCloud); ++resolved;
    steam::CloudReport result; result.state = steam::CloudState::kSynced; return result;
  }
};
struct LaunchHost : ww::steam_integration::ReadyLaunchHost {
  std::shared_ptr<homebrew::RuntimeSnapshot> snapshot = std::make_shared<homebrew::RuntimeSnapshot>();
  int requests=0, handoffs=0;
  std::shared_ptr<const homebrew::RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Play(std::uint32_t app) override { assert(app==449); ++requests; }
  bool Handoff() override { ++handoffs; return true; }
};
struct PrepareHost : ww::steam_integration::PreparationHost {
  std::shared_ptr<homebrew::RuntimeSnapshot> snapshot;
  int requests = 0;
  explicit PrepareHost(std::shared_ptr<homebrew::RuntimeSnapshot> s) : snapshot(std::move(s)) {}
  std::shared_ptr<const homebrew::RuntimeSnapshot> Snapshot() const override { return snapshot; }
  void Prepare(homebrew::RuntimePrepareRequest request) override {
    assert(request.runtime_number == 449 && request.game == GameId::Steam(449));
    assert(!request.install_path.empty() && !request.volume_id.empty());
    assert(request.launches.size() == 1 && request.launches[0].executable == "game.exe");
    ++requests; snapshot->queued_prepares.push_back(449);
  }
};
struct Installer : installs::InstallRunner {
  std::atomic<int> started{0}, removed{0}; std::atomic<bool> finish{false}, fail_removal{false};
  Result Install(const GameId&, const std::string&, const std::atomic<bool>& cancel,
                 const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>& progress,
                 const std::function<void(installs::InstallStep)>&) override {
    ++started; progress(30, 100, 30);
    while (!cancel && !finish) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (cancel) return {false,true,"",0,false,""};
    return {true,false,"",100,false,""};
  }
  std::string Remove(const std::string&) override { ++removed; return fail_removal ? "Cleanup failed." : ""; }
};
template<class F> void wait(ww::Shell& shell, F done) {
  for (int i=0; i<500 && !done(); ++i) { shell.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
  shell.tick(); assert(done());
}
static void click(ww::Shell& shell, int id) {
  shell.draw();
  for (int i=0; i<shell.count; ++i) if (shell.hits[i].id==id) { auto r=shell.hits[i].r; shell.pointer(r.x+5,r.y+5,true); return; }
  std::fprintf(stderr, "Missing UI control %d (window=%d file_games=%d downloads=%d panel=%d count=%d)\n", id, shell.window, shell.file_games, shell.library_downloads, shell.steam_panel, shell.count); assert(false);
}
int main() {
  char path[] = "/tmp/wine11-ui-install-test-XXXXXX"; assert(mkdtemp(path));
  {
  Installer installer; installs::InstallService install_service(installer, path);
  ww::steam_integration::traffic::Registry traffic;
  ww::steam_integration::DownloadCoordinator downloads(install_service, &traffic);
  for (int i=0; i<1000 && downloads.Busy(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(!downloads.Busy());
  Session sessions; Auth auth_backend; Library library_backend;
  steam::SteamService auth(auth_backend, sessions); LibraryService library(library_backend, sessions);
  ww::steam_integration::Controller controller(auth,library); ww::steam_integration::UiAdapter ui(controller); ui.attach_downloads(&downloads);
  DetailsSource info_source; steam::AppDetailsService info_service(info_source, sessions); ui.attach_game_details(&info_service);
  std::atomic<bool> remembered{false};
  ww::steam_integration::RememberSetting remember(false, [&](bool enabled) { remembered = enabled; return true; });
  assert(ui.attach_remember(&remember));
  ww::Shell shell; assert(shell.init("assets")); shell.steam=&ui; shell.activate(35);
  click(shell,970); click(shell,983);
  wait(shell,[&]{ return remembered && ui.status().remember_enabled && !ui.status().remember_busy; });
  click(shell,971);
  wait(shell,[&]{ return ui.status().qr_size>0; });
  assert(ui.status().account == ww::SteamAccount::approval);
  auth_backend.approved=true;
  wait(shell,[&]{ return ui.status().library == ww::SteamLibrary::ready; });
  assert(ui.status().game_count==10 && ui.status().qr_size==0);
  ui.request_artwork(440);
  wait(shell,[&]{ ww::SteamGame info; return ui.game(0, info) && info.has_install_bytes; });
  ww::SteamGame info; assert(ui.game(0, info) && info.install_bytes == 123456789 && info.has_download_bytes && info.download_bytes == 98765432);
  // Explicit refresh must also retry previously unavailable app details.
  info_source.unavailable = true; ui.refresh(); ui.request_artwork(440);
  wait(shell,[&]{ return info_source.calls >= 2; });
  info_source.unavailable = false; ui.refresh(); ui.request_artwork(440);
  wait(shell,[&]{ ww::SteamGame recovered; return ui.game(0, recovered) && recovered.has_install_bytes; });
  assert(info_source.calls >= 3);
  ww::SteamGame stats; assert(ui.game(0, stats));
  assert(stats.has_playtime && stats.playtime_minutes == 125 && stats.has_last_played && stats.last_played_unix_seconds == 1700000000);
  CloudRunner cloud_runner; steam::CloudService cloud_service(cloud_runner, sessions);
  ww::steam_integration::CloudPreflight cloud_check(cloud_service); assert(ui.attach_cloud_preflight(&cloud_check));
  steam::CloudLocation cloud_location; cloud_location.id = GameId::Steam(449);
  cloud_location.game_root = path; cloud_location.state_root = path; cloud_location.prefix_root = path;
  assert(cloud_check.Begin(cloud_location, 42, controller.view().account_generation));
  wait(shell,[&]{ return cloud_runner.entered.load(); });
  assert(!ui.play(449) && std::string(ui.status().message).find("Wait for Cloud") != std::string::npos);
  ui.sign_out(); assert(ui.status().account == ww::SteamAccount::online && cloud_check.Busy());
  cloud_runner.hold = false;
  wait(shell,[&]{ return !cloud_check.Busy(); });
  assert(ui.attach_cloud_preflight(nullptr));
  assert(!ui.install(999999));
  click(shell,974); click(shell,1009);
  assert(shell.steam_selection==9 && shell.pending==ww::Action::none);
  // A previously observed Playden installation must not be downloaded again.
  shell.game_inventory.count = 1; shell.game_inventory.entries[0].steam_app = 449;
  shell.game_inventory.entries[0].state = ww::games::State::unavailable;
  shell.activate(978); assert(installer.started == 0 && !downloads.Busy());
  shell.game_inventory.count = 0;
  click(shell,978);
  wait(shell,[&]{ ww::SteamDownload job; return ui.status().download_count == 1 && installer.started == 1 && ui.download(0, job) && job.state == ww::SteamInstallState::running; });
  // Transfer progress is the worker's completed chunk count, not installed size.
  wait(shell,[&]{ ww::SteamDownload job; return ui.download(0, job) && job.done == 30 && job.total == 100; });
  ww::SteamDownload progress; assert(ui.download(0, progress) && !progress.has_eta);
  // The UI rate comes from received payload, not the chunk-completion progress.
  auto attempt = traffic.begin(449);
  { ww::steam_integration::traffic::Scope scope(attempt); ww::steam_integration::traffic::received(1); }
  shell.tick();
  { ww::steam_integration::traffic::Scope scope(attempt); ww::steam_integration::traffic::received(1000); }
  const auto rate_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  do { shell.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); ui.download(0,progress); }
  while (!progress.has_rate && std::chrono::steady_clock::now() < rate_deadline);
  assert(progress.has_rate && progress.bytes_per_second > 0 && progress.done == 30);
  traffic.end(attempt);
  assert(!ui.attach_downloads(nullptr));
  ui.sign_in(); assert(ui.status().account == ww::SteamAccount::online);
  click(shell,970); click(shell,973);
  assert(ui.status().account == ww::SteamAccount::online && ui.status().message[0]);
  assert(shell.steam_selection == 9 && shell.steam_scroll == 0);
  click(shell,974); click(shell,966);
  click(shell,1105);
  wait(shell,[&]{ ww::SteamDownload job; return ui.download(0,job) && job.state == ww::SteamInstallState::paused; });
  click(shell,1100);
  wait(shell,[&]{ ww::SteamDownload job; return installer.started >= 2 && ui.download(0, job) && job.state == ww::SteamInstallState::running; });
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  installer.fail_removal = true;
  click(shell,1125);
  wait(shell,[&]{ ww::SteamDownload item; return installer.removed == 1 && ui.download(0,item) && item.cleanup_failure && item.cleanup_retryable; });
  assert(downloads.Busy() && !downloads.SafeToExit());
  installer.fail_removal = false;
  click(shell,1100);
  wait(shell,[&]{ return installer.removed == 2 && !downloads.Busy(); });
#else
  click(shell,1125);
  wait(shell,[&]{ return installer.removed == 1 && !downloads.Busy(); });
#endif
  click(shell,965); click(shell,1009); click(shell,978);
  wait(shell,[&]{ return installer.started >= 3; });
  installer.finish = true;
  wait(shell,[&]{ ww::SteamDownload item; return ui.download(0,item) && item.state == ww::SteamInstallState::installed; });
  ww::SteamDownload completed; assert(ui.download(0, completed));
  // The installer returned 100 installed bytes; this is not transfer progress.
  assert(completed.app_id == 449 && completed.done == 0 && completed.total == 0);
  assert(ui.attach_cloud_preflight(&cloud_check));
  cloud_location.game_root = std::string(path) + "/old-location";
  cloud_runner.answer = steam::CloudState::kConflict;
  assert(cloud_check.Begin(cloud_location, 42, controller.view().account_generation));
  wait(shell,[&]{ return !cloud_check.Busy(); });
  assert(ui.status().cloud_choice_app == 0 && !ui.resolve_cloud(449, true));
  cloud_location.game_root = std::string(path) + "/449";
  assert(cloud_check.Begin(cloud_location, 42, controller.view().account_generation));
  wait(shell,[&]{ return ui.status().cloud_choice_app == 449; });
  click(shell,981);
  wait(shell,[&]{ return cloud_runner.resolved == 1 && !cloud_check.Busy(); });
  assert(std::string(ui.status().message).find("resolved") != std::string::npos);
  {
    LaunchHost skip_host; skip_host.snapshot->status = homebrew::RuntimeHostStatus::kReady;
    skip_host.snapshot->kit_available = true;
    homebrew::RuntimeInstallView prepared; prepared.game = GameId::Steam(449); prepared.runtime_number = 449;
    prepared.install_path = cloud_location.game_root; prepared.state = runtime::PreparationState::kReady;
    skip_host.snapshot->installs.push_back(prepared);
    ww::steam_integration::ReadyLaunch skip_launch(skip_host, [&](std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
      return cloud_check.Poll(app, account, generation);
    });
    assert(ui.attach_ready_launch(&skip_launch));
    cloud_runner.answer = steam::CloudState::kUnsupported;
    assert(cloud_check.Begin(cloud_location, 42, controller.view().account_generation));
    wait(shell,[&]{ return ui.status().cloud_skip_app == 449; });
    click(shell,982); assert(skip_host.requests == 1);
    skip_host.snapshot->plays_handled = 1;
    homebrew::RuntimeLaunchView failure; failure.game = GameId::Steam(449); failure.state = runtime::LaunchState::kFailed;
    skip_host.snapshot->launch = failure; shell.tick();
    assert(!skip_launch.Busy() && ui.attach_ready_launch(nullptr));
  }
  assert(ui.attach_cloud_preflight(nullptr));
  click(shell,966); click(shell,1100);
  wait(shell,[&]{ return ui.status().download_count == 0; });
  LaunchHost host; host.snapshot->status=homebrew::RuntimeHostStatus::kReady;
  host.snapshot->installs_known=true; host.snapshot->kit_available=true;
  homebrew::RuntimeInstallView record; record.game=GameId::Steam(449); record.runtime_number=449;
  record.install_path=std::string(path)+"/449"; record.state=runtime::PreparationState::kReady;
  record.admission="ok";
  ww::steam_integration::CloudGateState cloud=ww::steam_integration::CloudGateState::blocked;
  ww::steam_integration::ReadyLaunch launch(host, [&](std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
    assert(app==449 && account==42 && generation>0);
    return ww::steam_integration::CloudGateResult{cloud,"Cloud check unavailable"};
  });
  PrepareHost prepare_host(host.snapshot); ww::steam_integration::Preparation prepare(prepare_host);
  assert(!ui.attach_game_runtime(&launch, nullptr));
  assert(ui.attach_game_runtime(&launch, &prepare));
  DetailsSource details_source; steam::AppDetailsService details_service(details_source, sessions);
  ww::steam_integration::LaunchDetails launch_details(details_service); assert(ui.attach_launch_details(&launch_details));
  click(shell,965); click(shell,1009); click(shell,979);
  wait(shell,[&]{ return prepare_host.requests == 1; });
  assert(prepare_host.requests==1 && prepare.Busy() && host.requests==0);
  ui.sign_out(); assert(ui.status().account==ww::SteamAccount::online);
  host.snapshot->queued_prepares.clear(); ++host.snapshot->prepares_handled; host.snapshot->installs.push_back(record);
  shell.tick();
  assert(host.requests==0 && launch.view().state==ww::steam_integration::ReadyLaunchState::failed);
  cloud=ww::steam_integration::CloudGateState::ready; click(shell,979);
  wait(shell,[&]{ return host.requests==1; });
  assert(host.requests==1 && launch.Busy() && shell.pending==ww::Action::none);
  ui.sign_out(); assert(ui.status().account==ww::SteamAccount::online);
  host.snapshot->plays_handled=1; homebrew::RuntimeLaunchView pending;
  pending.game=GameId::Steam(449); pending.state=runtime::LaunchState::kReadyToHandoff;
  host.snapshot->launch=pending; host.snapshot->handoff_ready=true;
  shell.tick(); assert(host.handoffs==1 && launch.Busy());
  host.snapshot->launch->state=runtime::LaunchState::kFailed; host.snapshot->launch->message="Native refusal";
  shell.tick(); assert(launch.view().state==ww::steam_integration::ReadyLaunchState::failed);
  details_source.entered = false; details_source.hold = true;
  click(shell,979);
  wait(shell,[&]{ return details_source.entered.load(); });
  // A blocked account change must not cancel another game's launch lookup.
  installer.finish = false;
  assert(downloads.Install(440));
  wait(shell,[&]{ return installer.started >= 4 && downloads.Busy(); });
  ui.sign_out();
  assert(ui.status().account == ww::SteamAccount::online && !details_source.canceled.load());
  assert(launch_details.state() == ww::steam_integration::LaunchDetailsState::waiting);
  ui.sign_in();
  assert(ui.status().account == ww::SteamAccount::online && !details_source.canceled.load());
  assert(launch_details.state() == ww::steam_integration::LaunchDetailsState::waiting);
  assert(downloads.Cancel(440));
  wait(shell,[&]{ return !downloads.Busy(); });
  ui.sign_out();
  wait(shell,[&]{ return ui.status().account == ww::SteamAccount::signed_out && details_source.canceled.load(); });
  assert(prepare_host.requests == 1 && host.requests == 1);
  assert(ui.attach_ready_launch(nullptr));
  assert(ui.attach_preparation(nullptr));
  assert(ui.attach_launch_details(nullptr));
  details_source.hold = false;
  assert(ui.status().game_count==0 && !sessions.Load().value());
  // Restored work cannot prevent the first sign-in that makes it runnable.
  installer.finish = false;
  assert(downloads.Install(440));
  wait(shell,[&]{ return installer.started >= 5 && downloads.Busy(); });
  auth_backend.fail_once = true;
  ui.sign_in();
  wait(shell,[&]{ return ui.status().account == ww::SteamAccount::failed; });
  ui.sign_in();
  wait(shell,[&]{ return ui.status().account == ww::SteamAccount::online; });
  assert(downloads.Cancel(440));
  wait(shell,[&]{ return !downloads.Busy(); });
  ui.sign_out();
  wait(shell,[&]{ return ui.status().account == ww::SteamAccount::signed_out; });
  assert(ui.attach_downloads(nullptr));
  assert(ui.attach_remember(nullptr));
  shell.steam=nullptr;
  }
  std::filesystem::remove_all(path);
  std::puts("PASS: desktop to Playden workers: sign-in, library, install, completion, cloud-gated launch, native failure and guarded sign-out");
}
