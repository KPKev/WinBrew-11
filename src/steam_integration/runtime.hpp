// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#if defined(WINE11_EXPERIMENT_UNINSTALL)
#include "uninstall_journal.hpp"
#endif
namespace ww { class SteamUi; }
namespace playden::platform { class ManagedDirectory; class ManagedFileLock; }
namespace playden::homebrew { class RuntimeHost; }
namespace ww::steam_integration {
class Controller;
class ReadyLaunch;
class Preparation;
class CloudPreflight;
class ControllerLayoutStore;
class TitleRemoval;
enum class UninstallFinish;
// Owns the real Playden account and library services. The caller supplies a
// Wine 11-only private state directory and verified CA bundle. It must keep the
// object alive until all UI access ends and must never share a writer with Playden.
class Runtime {
 public:
  Runtime(std::string private_root, std::string verified_ca_bundle, bool persist_session);
  ~Runtime();
  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;
  Controller& controller();
  ww::SteamUi& ui();
  // The application owns the configured RuntimeHost, cloud gate and coordinator;
  // all must outlive Runtime. Attach only on the application thread.
  bool AttachReadyLaunch(ReadyLaunch*);
  bool AttachPreparation(Preparation*);
  bool AttachGameRuntime(ReadyLaunch*, Preparation*);
  bool AttachControls(ControllerLayoutStore* store, std::function<void(std::uint32_t)> apply);
  // Application thread only; initialize when the native game runtime needs Cloud checks.
  CloudPreflight* EnableCloud();
  // Initialize on a startup worker, before exposing ui() to the presentation
  // thread. The caller supplies an existing private directory named
  // wine11-games with a pre-created private wine11-install.lock file. The
  // directory must be dedicated to this Wine 11 title, not a Playden root.
  // Acquires its own exclusive lease and retains it until shutdown. The
  // directory is not consumed on refusal.
  bool EnableDownloads(playden::platform::ManagedDirectory&&
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY) || defined(WINE11_EXPERIMENT_UNINSTALL)
      , const playden::platform::ManagedDirectory&, const playden::platform::ManagedFileLock&
#endif
  );
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  bool AttachGameRemoval(playden::homebrew::RuntimeHost& host);
  // Application thread only. A successful pause detaches and stops the
  // exclusive install-service worker before any registry maintenance.
  bool PauseDownloadsForRemoval();
  bool CancelEmptyRemoval();
  bool PrepareRemovedGame(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account);
  bool RecordTitleRemoved(const TitleRemoval& removal);
  bool RetireRemovedGame();
  bool ReconcileRemovedGame();
  bool CleanupRemovedGame();
  UninstallJournal::Entry CurrentRemoval() const;
  bool RecordRecoveredTitleAbsent(playden::homebrew::RuntimeHost& host);
  bool RecoverRemovedGame();
  bool FinishRemovedGame();
  UninstallFinish FinishReinstalledGame(std::uint64_t account);
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  bool ClaimOwnerRetireRecovery(const UninstallJournal::Entry& entry);
#endif
#endif
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}
