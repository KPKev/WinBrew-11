// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.hpp"
#include "controller.hpp"
#include "ui_adapter.hpp"
#include "guarded_runner.hpp"
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
extern "C" { [[gnu::used]] int wine11_cancel_guard_runtime_enabled = 1; }
#endif
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
#include "cancel_recovery_capability.hpp"
#include "cancel_cleanup_journal.hpp"
#include "cancel_registry_reconciliation.hpp"
#include "platform/random_id.h"
#include <charconv>
#endif
#include "download_tuning_store.hpp"
#include "cloud_runtime.hpp"
#include "remember_setting.hpp"
#include "artwork.hpp"
#include "library/cache.h"
#include "steam/backend/auth_client.h"
#include "steam/backend/cm_client.h"
#include "steam/backend/cm_app_details.h"
#include "steam/backend/entitled_library.h"
#include "steam/backend/library_client.h"
#include "steam/backend/cdn_client.h"
#include "steam/backend/steam_install_runner.h"
#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "steam/local_session_repository.h"
#include "steam/session_vault.h"
#if defined(WINE11_EXPERIMENT_UNINSTALL)
#include "uninstall_journal.hpp"
#include "uninstall_registry.hpp"
#include "protected_uninstall.hpp"
#include "title_removal.hpp"
#include "uninstall_flow.hpp"
#include "native_title_absence.hpp"
#include "vapor_title_remover.hpp"
#if defined(WINE11_PAYLOAD_GAME_CLEANUP)
#include "vapor_game_cleanup.hpp"
#include "platform/ps5/title_remover.h"
#include <chrono>
#include <thread>
extern "C" const unsigned char vapor_game_cleaner[];
extern "C" const std::size_t vapor_game_cleaner_size;
#endif
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
#include "owner_blender_request.hpp"
#include <ctime>
#endif
#if defined(WINE11_OWNER_INSPECT_ONLY)
#include "owner_blender_inspection.hpp"
#include <chrono>
#endif
#if defined(WINE11_OWNER_CLEANUP_PROBE)
#include "owner_cleanup_probe.hpp"
#endif
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
#include "owner_title_recovery.hpp"
#include <ctime>
#endif
#include "runtime/artifact_hash.h"
#include "platform/random_id.h"
#include <unistd.h>
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
#error "Uninstall recovery and experimental cancellation cannot share the worker yet"
#endif
#endif
#include <utility>
#include <cstdio>
#include <cstring>
namespace ww::steam_integration {
#if defined(WINE11_EXPERIMENT_UNINSTALL)
class RuntimeUninstallOperations final : public UninstallOperations {
 public:
  RuntimeUninstallOperations(Runtime& runtime, playden::homebrew::RuntimeHost& host) : runtime_(runtime), host_(host) {}
  bool Pause() override { return runtime_.PauseDownloadsForRemoval(); }
  bool CancelEmpty() override { return runtime_.CancelEmptyRemoval(); }
  bool Prepare(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account) override {
    return runtime_.PrepareRemovedGame(app, title, reinstall, account);
  }
  bool VerifyTitle(const TitleRemoval& title) override { return VerifyNativeTitleAbsent(host_, title.title()) == TitleObservation::absent; }
  bool RecordTitle(const TitleRemoval& title) override { return runtime_.RecordTitleRemoved(title); }
  bool Retire() override { return runtime_.RetireRemovedGame(); }
  bool Reconcile() override { return runtime_.ReconcileRemovedGame(); }
  bool Cleanup() override { return runtime_.CleanupRemovedGame(); }
  UninstallJournal::Entry Recovery() override { return runtime_.CurrentRemoval(); }
  bool RecoverTitle() override { return runtime_.RecordRecoveredTitleAbsent(host_); }
  bool RecoverWork() override { return runtime_.RecoverRemovedGame(); }
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  bool ClaimOwnerRetireRecovery(const UninstallJournal::Entry& entry) override { return runtime_.ClaimOwnerRetireRecovery(entry); }
#endif
  UninstallFinish Finish(bool reinstall, std::uint64_t account) override {
    return reinstall ? runtime_.FinishReinstalledGame(account)
                     : runtime_.FinishRemovedGame() ? UninstallFinish::complete : UninstallFinish::failed;
  }
 private:
  Runtime& runtime_;
  playden::homebrew::RuntimeHost& host_;
};
#endif
class Runtime::Impl {
 public:
  Impl(std::string root, std::string ca, bool persist)
      : private_root(root), ca_bundle(ca), storage(root), vault(persist), auth_sessions(root, {}, &vault),
        library_sessions(root, {}, &vault), details_sessions(root, {}, &vault), info_sessions(root, {}, &vault),
        remember(persist, [this](bool value) {
          const auto uid = playden::ps5::HoldEffectiveUid();
          return auth_sessions.SetPersist(value).ok();
        }),
        shared(playden::steam::MakeCmSharedSession()),
        auth_cm(ca, storage), library_cm(ca, storage), details_cm(ca, storage), info_cm(ca, storage), auth(ca), owned(ca),
        entitled(owned, library_cm), details_source(details_cm), info_source(info_cm), cache(storage),
        auth_service(auth, auth_sessions), library_service(entitled, library_sessions, &cache),
        details_service(details_source, details_sessions), info_service(info_source, info_sessions), controller(auth_service, library_service),
        launch_details(details_service), artwork(ca, [] { return std::chrono::steady_clock::now(); }, {
          [this](const std::string& key, std::size_t maximum) {
            const auto uid = playden::ps5::HoldEffectiveUid();
            auto bytes = storage.Read(key, maximum);
            return bytes.ok() ? std::move(bytes.value()) : playden::Bytes{};
          },
          [this](const std::string& key, const playden::Bytes& bytes) {
            const auto uid = playden::ps5::HoldEffectiveUid();
            if (storage.EnsureRootDirectory().ok()) (void)storage.WriteAtomic(key, bytes);
          },
          [this](const std::string& key) {
            const auto uid = playden::ps5::HoldEffectiveUid();
            (void)storage.Remove(key);
          }
        }), ui(controller) {
    auth_cm.SetOwner("wine11-auth");
    library_cm.SetOwner("wine11-library");
    details_cm.SetOwner("wine11-details");
    info_cm.SetOwner("wine11-game-info");
    auth_cm.SetSharedSession(shared);
    library_cm.SetSharedSession(shared);
    details_cm.SetSharedSession(shared);
    info_cm.SetSharedSession(shared);
    entitled.SetLicensedDepots(&licensed);
    details_source.SetLicensedDepots(&licensed);
    info_source.SetLicensedDepots(&licensed);
    ui.attach_launch_details(&launch_details);
    ui.attach_artwork(&artwork);
    ui.attach_game_details(&info_service);
    auth.SetMachineId([this] {
      return playden::steam::MakeDeviceIdentity(playden::steam::LoadOrCreateDeviceSeed(storage)).machine_id;
    });
    auth.SetCmRenewal([this](const playden::StoredSession& session, const std::atomic<bool>& cancel) {
      return playden::steam::RenewThroughCm(auth_cm, session, cancel);
    });
    ui.attach_remember(&remember);
    if (persist) controller.Restore();
    else controller.SignOut();
  }
  // Declaration order is also the reverse shutdown order: workers stop before
  // their backends, shared session, credential vault and storage are destroyed.
  struct DownloadServices {
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
    static std::uint64_t RandomOperationPart() {
      const auto uuid = playden::RandomUuid();
      if (!uuid) return 0;
      std::string digits;
      for (const char c : *uuid) if (c != '-') digits.push_back(c);
      if (digits.size() != 32) return 0;
      std::uint64_t value = 0;
      const auto parsed = std::from_chars(digits.data(), digits.data() + 16, value, 16);
      return parsed.ec == std::errc{} && parsed.ptr == digits.data() + 16 ? value : 0;
    }
    bool PrepareCancellation(playden::GameId id) {
      const auto app = id.steam_app();
      const auto operation = RandomOperationPart(), nonce = RandomOperationPart();
      CancelTargetEvidence target;
      if (!app || !operation || !nonce || !recovery.Valid() ||
          !InspectCancelTarget(directory, lease, *app, nonce, target)) return false;
      if (!recovery_journal.Prepare(*app, operation, target)) {
        const auto existing = recovery_journal.Read();
        if (existing.state != CancelCleanupJournal::State::prepared || existing.app != *app ||
            existing.operation != operation || existing.target.nonce != nonce) return false;
      }
      // A failed marker sync may still leave a complete marker. Binding
      // independently revalidates and syncs it; incomplete markers fail closed.
      (void)recovery_journal.CaptureMarker(*app, operation);
      if (recovery_journal.BindMarker(*app, operation)) return true;
      const auto existing = recovery_journal.Read();
      return existing.state == CancelCleanupJournal::State::intent && existing.app == *app &&
          existing.operation == operation && existing.target.nonce == nonce;
    }
#endif
    static playden::steam::SteamInstallRunner& ConfigureRunner(playden::steam::SteamInstallRunner& runner, const std::string& root) {
      const auto setting = root + "/vapor-download-slots-v2.txt";
      const auto logfile = root + "/vapor-download-tuning.log";
      runner.SetTuning([setting] {
        playden::steam::DownloadTuning tuning; tuning.slots = ReadTunedSlots(setting); return tuning;
      });
      runner.SetLog([setting, logfile](std::uint32_t app, const char* step, const char* category, const char*) {
        RecordTuning(setting, logfile, app, step, category);
      });
      return runner;
    }
    static std::unique_ptr<playden::steam::CmClient> MakeCm(Impl& parent) {
      auto cm = std::make_unique<playden::steam::CmClient>(parent.ca_bundle, parent.storage);
      cm->SetOwner("wine11-install"); cm->SetSharedSession(parent.shared);
      return cm;
    }
    DownloadServices(playden::platform::ManagedDirectory&& directory_arg,
                     playden::platform::ManagedFileLock&& lease_arg,
                     Impl& parent
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY) || defined(WINE11_EXPERIMENT_UNINSTALL)
                     , const playden::platform::ManagedDirectory& state, const playden::platform::ManagedFileLock& state_lease
#endif
                     )
        : directory(std::move(directory_arg)), lease(std::move(lease_arg)),
          sessions(parent.private_root, {}, &parent.vault), cm(MakeCm(parent)),
          cdn(parent.ca_bundle), runner(sessions, *cm, cdn), guarded(directory, lease, ConfigureRunner(runner, parent.private_root), &traffic),
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
          recovery(state, state_lease), recovery_journal(state, state_lease, directory, lease),
          service({{playden::Store::kSteam, &guarded}}, {{"internal", "Console storage", directory.path().string(), true}},
                  [this] {
                    if (recovery.Valid()) {
                      const auto entry = recovery_journal.Read();
                      if (entry.state == CancelCleanupJournal::State::intent)
                        (void)ReconcileCanceledQueueBeforeScan(recovery_journal, directory, lease);
                    }
                    // Reconciliation and deletion are not implemented: remain blocked.
                    return false;
                  }
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
                  , [this](playden::GameId id) { return PrepareCancellation(std::move(id)); }
#endif
                  ), coordinator(service, &traffic) {}
#elif defined(WINE11_EXPERIMENT_UNINSTALL)
          uninstall_journal(state, state_lease, directory, lease) {
      if (uninstall_journal.Read().phase == UninstallJournal::Phase::absent) StartService();
    }
#else
          service(guarded, directory.path().string()), coordinator(service, &traffic) {}
#endif
    playden::platform::ManagedDirectory directory;
    playden::platform::ManagedFileLock lease;
    playden::LocalSessionRepository sessions;
    std::unique_ptr<playden::steam::CmClient> cm;
    playden::steam::CdnClient cdn;
    playden::steam::SteamInstallRunner runner;
    traffic::Registry traffic;
    GuardedInstallRunner guarded;
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
    CancelRecoveryCapability recovery;
    CancelCleanupJournal recovery_journal;
#endif
#if defined(WINE11_EXPERIMENT_UNINSTALL)
    UninstallJournal uninstall_journal;
    std::unique_ptr<playden::installs::InstallService> service;
    std::unique_ptr<DownloadCoordinator> coordinator;
    std::optional<playden::installs::InstallSnapshot> maintenance_snapshot;
    void StartService() {
      service = std::make_unique<playden::installs::InstallService>(guarded, directory.path().string());
      coordinator = std::make_unique<DownloadCoordinator>(*service, &traffic);
    }
#else
    playden::installs::InstallService service;
    DownloadCoordinator coordinator;
#endif
  };
  std::string private_root, ca_bundle;
  playden::Storage storage;
  playden::SessionVault vault;
  playden::LocalSessionRepository auth_sessions, library_sessions, details_sessions, info_sessions;
  RememberSetting remember;
  std::shared_ptr<playden::steam::CmSharedSession> shared;
  playden::steam::CmClient auth_cm, library_cm, details_cm, info_cm;
  playden::steam::AuthClient auth;
  playden::steam::LibraryClient owned;
  playden::steam::LicensedDepots licensed;
  playden::steam::EntitledLibraryBackend entitled;
  playden::steam::CmAppDetailsSource details_source;
  playden::steam::CmAppDetailsSource info_source;
  playden::LibraryCache cache;
  playden::steam::SteamService auth_service;
  playden::LibraryService library_service;
  playden::steam::AppDetailsService details_service;
  playden::steam::AppDetailsService info_service;
  Controller controller;
  LaunchDetails launch_details;
  ArtworkService artwork;
  UiAdapter ui;
  std::unique_ptr<CloudRuntime> cloud;
  // Destroy download services before the UI, session vault and shared CM.
  std::unique_ptr<DownloadServices> downloads;
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  std::unique_ptr<PlaydenTitleRemovalHost> title_removal_host;
  std::unique_ptr<TitleRemoval> title_removal;
  std::unique_ptr<RuntimeUninstallOperations> uninstall_operations;
  std::unique_ptr<UninstallFlow> uninstall_flow;
#endif
};
Runtime::Runtime(std::string root, std::string ca, bool persist)
    : impl_(std::make_unique<Impl>(std::move(root), std::move(ca), persist)) {}
Runtime::~Runtime() = default;
Controller& Runtime::controller() { return impl_->controller; }
ww::SteamUi& Runtime::ui() { return impl_->ui; }
bool Runtime::AttachReadyLaunch(ReadyLaunch* launch) { return impl_->ui.attach_ready_launch(launch); }
bool Runtime::AttachPreparation(Preparation* preparation) { return impl_->ui.attach_preparation(preparation); }
bool Runtime::AttachGameRuntime(ReadyLaunch* launch, Preparation* preparation) { return impl_->ui.attach_game_runtime(launch, preparation); }
bool Runtime::AttachControls(ControllerLayoutStore* store, std::function<void(std::uint32_t)> apply) {
  return impl_->ui.attach_controls(store, std::move(apply));
}
CloudPreflight* Runtime::EnableCloud() {
  if (!impl_->cloud)
    impl_->cloud = std::make_unique<CloudRuntime>(impl_->private_root, impl_->ca_bundle,
                                                  impl_->storage, impl_->vault, impl_->shared);
  auto* preflight = &impl_->cloud->preflight();
  return impl_->ui.attach_cloud_preflight(preflight) ? preflight : nullptr;
}
bool Runtime::EnableDownloads(playden::platform::ManagedDirectory&& directory
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY) || defined(WINE11_EXPERIMENT_UNINSTALL)
    , const playden::platform::ManagedDirectory& state, const playden::platform::ManagedFileLock& state_lease
#endif
    ) {
  using playden::platform::DirectoryPolicy;
  using playden::platform::LockMode;
  using playden::platform::ManagedFileLock;
  if (impl_->downloads || impl_->ui.downloads_attached() || directory.policy() != DirectoryPolicy::kPrivate ||
      !directory.path().is_absolute() || directory.path().filename() != "wine11-games" ||
      !directory.Validate()) return false;
  auto lease = ManagedFileLock::Acquire(directory, "wine11-install.lock", LockMode::kPublisher);
  if (!lease || !lease->Validate(directory, "wine11-install.lock")) return false;
  impl_->downloads = std::make_unique<Impl::DownloadServices>(std::move(directory), std::move(*lease), *impl_
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY) || defined(WINE11_EXPERIMENT_UNINSTALL)
      , state, state_lease
#endif
  );
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  return impl_->downloads->coordinator ? impl_->ui.attach_downloads(impl_->downloads->coordinator.get())
                                       : impl_->ui.begin_download_recovery();
#else
  return impl_->ui.attach_downloads(&impl_->downloads->coordinator);
#endif
}
#if defined(WINE11_EXPERIMENT_UNINSTALL)
bool Runtime::AttachGameRemoval(playden::homebrew::RuntimeHost& host) {
  if (impl_->uninstall_flow || !impl_->downloads) {
    impl_->ui.uninstall_unavailable("Removal downloads service unavailable"); return false;
  }
  std::string reason;
  if (!VerifyVaporTitleRemover(kVaporTitleRemoverPath, &reason)) {
    impl_->ui.uninstall_unavailable(reason); return false;
  }
  impl_->title_removal_host = std::make_unique<PlaydenTitleRemovalHost>(host);
  impl_->title_removal = std::make_unique<TitleRemoval>(*impl_->title_removal_host);
  impl_->uninstall_operations = std::make_unique<RuntimeUninstallOperations>(*this, host);
  impl_->uninstall_flow = std::make_unique<UninstallFlow>(*impl_->uninstall_operations, *impl_->title_removal);
  if (!impl_->ui.attach_uninstall(impl_->uninstall_flow.get())) {
    impl_->ui.uninstall_unavailable("Removal controls could not attach"); return false;
  }
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
  auto* downloads = impl_->downloads.get();
  impl_->ui.arm_owner_blender_request([downloads] {
    if (!downloads || downloads->uninstall_journal.Read().phase != UninstallJournal::Phase::absent)
      return OwnerBlenderClaim::rejected;
    return ClaimOwnerBlenderRequest(downloads->directory, downloads->lease, std::time(nullptr));
  });
#endif
#if defined(WINE11_OWNER_INSPECT_ONLY)
  auto* inspected_downloads = impl_->downloads.get();
  const auto inspection_started = std::chrono::steady_clock::now();
  impl_->ui.arm_owner_blender_inspection([this, inspected_downloads, &host, inspection_started] {
    if (!inspected_downloads) return true;
    const auto snapshot = host.Shared();
    const bool timed_out = std::chrono::steady_clock::now() - inspection_started > std::chrono::seconds(15);
    if (!snapshot || ((snapshot->status == playden::homebrew::RuntimeHostStatus::kStarting || !snapshot->installs_known) && !timed_out))
      return false;
    const bool ready = inspected_downloads->coordinator && inspected_downloads->coordinator->MaintenanceReady();
    if (inspected_downloads->coordinator && !ready && !timed_out) return false;
    std::optional<playden::installs::InstallSnapshot> installs;
    if (inspected_downloads->coordinator) installs = inspected_downloads->coordinator->Snapshot();
    (void)WriteOwnerBlenderInspection(inspected_downloads->directory, inspected_downloads->lease,
        inspected_downloads->uninstall_journal, impl_->storage, *snapshot,
        installs ? &*installs : nullptr, ready);
#if defined(WINE11_OWNER_CLEANUP_PROBE)
    if (inspected_downloads->uninstall_journal.Read().phase == UninstallJournal::Phase::absent && ready &&
        PauseDownloadsForRemoval()) {
      (void)RunOwnerCleanupProbe(inspected_downloads->directory, inspected_downloads->lease, "/data/wine11");
      (void)CancelEmptyRemoval();
    }
#endif
    return true;
  });
#endif
  return true;
}
bool Runtime::PauseDownloadsForRemoval() {
  auto* d = impl_->downloads.get();
  if (!d || !d->coordinator || !d->service || !d->coordinator->FreezeForMaintenance()) return false;
  d->maintenance_snapshot = d->coordinator->Snapshot();
  if (!impl_->ui.detach_downloads_for_maintenance()) { d->coordinator->ThawAfterMaintenance(); return false; }
  d->coordinator.reset(); // stop the snapshot worker before its service
  d->service.reset();     // joins the already-idle install worker
  return true;
}
bool Runtime::CancelEmptyRemoval() {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service || d->uninstall_journal.Read().phase != UninstallJournal::Phase::absent) return false;
  d->StartService();
  return d->coordinator && impl_->ui.finish_download_maintenance(d->coordinator.get());
}
bool Runtime::PrepareRemovedGame(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account) {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service || !d->maintenance_snapshot || !app || (reinstall && !account)) return false;
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto id = playden::GameId::Steam(app); unsigned matches = 0;
  for (const auto& game : d->maintenance_snapshot->installed) {
    if (game.id == id && game.available && game.volume_id == "internal" &&
        game.root == (d->directory.path() / std::to_string(app)).string()) ++matches;
  }
  for (const auto& job : d->maintenance_snapshot->jobs)
    if (job.state != playden::installs::InstallJobState::kPaused || job.id == id) return false;
  if (matches != 1 || !d->maintenance_snapshot->uninstalling.empty() ||
      !d->directory.Validate() || !d->lease.Validate(d->directory, "wine11-install.lock")) return false;
  struct stat info{};
  if (!d->directory.InspectChild(std::to_string(app), &info) || !S_ISDIR(info.st_mode) || info.st_uid != geteuid()) return false;
  const auto operation = playden::RandomUuid();
  const auto bytes = ReadInstalledRegistry(d->directory, d->lease);
  if (!operation || !bytes) return false;
  UninstallJournal::Entry entry;
  entry.phase = UninstallJournal::Phase::intent; entry.app = app; entry.operation = *operation;
  entry.root_device = info.st_dev; entry.root_inode = info.st_ino; entry.title = title;
  entry.reinstall = reinstall; entry.account = account;
  entry.registry_before = *bytes;
  return d->uninstall_journal.Begin(entry);
}
bool Runtime::RecordTitleRemoved(const TitleRemoval& removal) {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service || removal.state() != TitleRemovalState::removed) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::intent || entry.app != removal.app() || entry.title != removal.title()) return false;
  return d->uninstall_journal.Advance(entry, UninstallJournal::Phase::title_removed);
}
bool Runtime::RetireRemovedGame() {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::title_removed) return false;
  const UninstallTarget target{entry.app, entry.operation, entry.root_device, entry.root_inode};
  auto state = InspectProtectedGame(d->directory, d->lease, "/data/wine11", target);
  if (state == ProtectedState::original) state = RetireProtectedGame(d->directory, d->lease, "/data/wine11", target);
  if (state != ProtectedState::retired) return false;
  return d->uninstall_journal.Advance(entry, UninstallJournal::Phase::game_retired);
}
bool Runtime::ReconcileRemovedGame() {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::game_retired) return false;
  const UninstallTarget target{entry.app, entry.operation, entry.root_device, entry.root_inode};
  if (InspectProtectedGame(d->directory, d->lease, "/data/wine11", target) != ProtectedState::retired) return false;
  const auto plan = PlanInstalledRegistryRow(entry.app, entry.registry_before);
  if (!plan) return false;
  const auto result = RemoveInstalledRegistryRow(d->directory, d->lease, *plan);
  if (result != RegistryRemoval::removed && result != RegistryRemoval::already_absent) return false;
  return d->uninstall_journal.Advance(entry, UninstallJournal::Phase::registry_removed);
}
bool Runtime::CleanupRemovedGame() {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::registry_removed) return false;
  const UninstallTarget target{entry.app, entry.operation, entry.root_device, entry.root_inode};
  auto state = InspectProtectedGame(d->directory, d->lease, "/data/wine11", target);
#if defined(WINE11_PAYLOAD_GAME_CLEANUP)
  const auto valid = [&] {
    const auto now = d->uninstall_journal.Read();
    return !d->coordinator && !d->service && now.phase == UninstallJournal::Phase::registry_removed &&
        now.app == entry.app && now.operation == entry.operation && now.root_device == entry.root_device &&
        now.root_inode == entry.root_inode && now.title == entry.title && now.reinstall == entry.reinstall &&
        now.account == entry.account && now.registry_before == entry.registry_before;
  };
  const auto observed = [&] { return InspectProtectedGame(d->directory, d->lease, "/data/wine11", target); };
  const auto dispatch = [] {
    if (vapor_game_cleaner_size < 4 || vapor_game_cleaner_size > 4 * 1024 * 1024) return false;
    const std::string elf(reinterpret_cast<const char*>(vapor_game_cleaner), vapor_game_cleaner_size);
    const auto hash = playden::runtime::HashArtifactBytes(elf);
    if (!hash || *hash != "0de8cbe5441aba99859af79857c50a8fad897c0e4cfee5511b1654266aaa3ea2") return false;
    std::string error; return playden::ps5::SendToPayloadLoader(elf, &error);
  };
  const auto wait = [] { std::this_thread::sleep_for(std::chrono::milliseconds(250)); };
  const vapor_cleaner::Request request{entry.app, entry.operation, entry.root_device, entry.root_inode, entry.title};
  if (state == ProtectedState::blocked ||
      RunGameCleanupHelper(d->directory, d->lease, request, valid, observed, dispatch, wait, 480) != CleanerState::complete) return false;
  state = observed();
#else
  if (state == ProtectedState::retired) state = RemoveProtectedGame(d->directory, d->lease, "/data/wine11", target);
#endif
  if (state != ProtectedState::removed) return false;
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  if (entry.app == 365670 && entry.title == "FAKE78008" && !entry.reinstall &&
      !CleanupOwnerTitleQuarantine(d->directory, d->lease, BlenderOwnerTitleRecovery())) return false;
#endif
  return d->uninstall_journal.Advance(entry, UninstallJournal::Phase::quarantine_removed);
}
UninstallJournal::Entry Runtime::CurrentRemoval() const {
  const auto* d = impl_->downloads.get();
  return d && !d->coordinator && !d->service ? d->uninstall_journal.Read() : UninstallJournal::Entry{};
}
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
bool Runtime::ClaimOwnerRetireRecovery(const UninstallJournal::Entry& expected) {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto live = d->uninstall_journal.Read();
  if (live.phase != expected.phase || live.app != expected.app || live.title != expected.title ||
      live.operation != expected.operation || live.root_device != expected.root_device ||
      live.root_inode != expected.root_inode || live.reinstall != expected.reinstall ||
      live.account != expected.account || live.registry_before != expected.registry_before) return false;
  struct stat game{};
  if (!d->directory.InspectChild("365670", &game) || !S_ISDIR(game.st_mode) || game.st_uid != geteuid() ||
      static_cast<std::uint64_t>(game.st_dev) != live.root_device ||
      static_cast<std::uint64_t>(game.st_ino) != live.root_inode) return false;
  return ClaimOwnerTitleRecovery(d->directory, d->lease, live, BlenderOwnerTitleRecovery(), std::time(nullptr));
}
#endif
bool Runtime::RecordRecoveredTitleAbsent(playden::homebrew::RuntimeHost& host) {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::intent ||
      VerifyNativeTitleAbsent(host, entry.title) != TitleObservation::absent) return false;
  return d->uninstall_journal.Advance(entry, UninstallJournal::Phase::title_removed);
}
bool Runtime::RecoverRemovedGame() {
  for (int step = 0; step < 4; ++step) {
    const auto entry = CurrentRemoval();
    if (entry.phase == UninstallJournal::Phase::title_removed) { if (!RetireRemovedGame()) return false; }
    else if (entry.phase == UninstallJournal::Phase::game_retired) { if (!ReconcileRemovedGame()) return false; }
    else if (entry.phase == UninstallJournal::Phase::registry_removed) { if (!CleanupRemovedGame()) return false; }
    else return entry.phase == UninstallJournal::Phase::quarantine_removed ||
                entry.phase == UninstallJournal::Phase::queue_intent || entry.phase == UninstallJournal::Phase::download_queued;
  }
  return false;
}
bool Runtime::FinishRemovedGame() {
  auto* d = impl_->downloads.get();
  if (!d || d->coordinator || d->service) return false;
  const auto entry = d->uninstall_journal.Read();
  if (entry.phase != UninstallJournal::Phase::quarantine_removed || entry.reinstall) return false;
  const UninstallTarget target{entry.app, entry.operation, entry.root_device, entry.root_inode};
  if (InspectProtectedGame(d->directory, d->lease, "/data/wine11", target) != ProtectedState::removed) return false;
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  if (entry.app == 365670 && entry.title == "FAKE78008" &&
      !OwnerTitleQuarantineAbsent(d->directory, d->lease, BlenderOwnerTitleRecovery())) return false;
#endif
  const auto plan = PlanInstalledRegistryRow(entry.app, entry.registry_before);
  if (!plan || RemoveInstalledRegistryRow(d->directory, d->lease, *plan) != RegistryRemoval::already_absent ||
      !d->uninstall_journal.Clear(entry)) return false;
  d->StartService();
  return d->coordinator && impl_->ui.finish_download_maintenance(d->coordinator.get());
}
UninstallFinish Runtime::FinishReinstalledGame(std::uint64_t account) {
  auto* d = impl_->downloads.get();
  if (!d || !account) return UninstallFinish::failed;
  auto entry = d->uninstall_journal.Read();
  if (!entry.reinstall || entry.account != account ||
      (entry.phase != UninstallJournal::Phase::quarantine_removed && entry.phase != UninstallJournal::Phase::queue_intent &&
       entry.phase != UninstallJournal::Phase::download_queued)) return UninstallFinish::failed;
  const auto& view = impl_->controller.view();
  if (view.account != AccountState::online || view.account_id != account || view.library != LibraryState::ready) {
    impl_->ui.allow_download_recovery_signin();
    return UninstallFinish::pending;
  }
  bool owned = false;
  for (const auto& game : view.games) if (game.id == playden::GameId::Steam(entry.app)) owned = true;
  if (!owned) return UninstallFinish::failed;
  const auto plan = PlanInstalledRegistryRow(entry.app, entry.registry_before);
  if (!plan) return UninstallFinish::failed;
  if (entry.phase == UninstallJournal::Phase::quarantine_removed) {
    const UninstallTarget target{entry.app, entry.operation, entry.root_device, entry.root_inode};
    if (d->service || d->coordinator || InspectProtectedGame(d->directory, d->lease, "/data/wine11", target) != ProtectedState::removed ||
        RemoveInstalledRegistryRow(d->directory, d->lease, *plan) != RegistryRemoval::already_absent ||
        !d->uninstall_journal.Advance(entry, UninstallJournal::Phase::queue_intent)) return UninstallFinish::failed;
    entry = d->uninstall_journal.Read();
    if (entry.phase != UninstallJournal::Phase::queue_intent) return UninstallFinish::failed;
  }
  if (d->service || d->coordinator) return UninstallFinish::failed;
  if (entry.phase == UninstallJournal::Phase::queue_intent) {
    const auto queued = QueueReinstallRegistry(d->directory, d->lease, *plan);
    if (queued != QueueReinstall::queued && queued != QueueReinstall::already_queued) return UninstallFinish::failed;
    if (!d->uninstall_journal.Advance(entry, UninstallJournal::Phase::download_queued)) return UninstallFinish::failed;
    entry = d->uninstall_journal.Read();
  }
  if (entry.phase != UninstallJournal::Phase::download_queued ||
      InspectReinstallRegistry(d->directory, d->lease, *plan) != ReinstallRegistry::queued ||
      !d->uninstall_journal.Clear(entry)) return UninstallFinish::failed;
  d->StartService();
  return impl_->ui.finish_download_maintenance(d->coordinator.get()) ? UninstallFinish::complete : UninstallFinish::failed;
}
#endif
}
