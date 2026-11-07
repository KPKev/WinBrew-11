// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_session.hpp"
#include "native_paths.hpp"
#include "runtime.hpp"
#include "native_game_runtime.hpp"
#include "native_kit_admission.hpp"
#include "cloud_location.hpp"
#include "controller_layouts.hpp"
#if defined(WINE11_EXPERIMENT_UNINSTALL)
#include "vapor_title_remover.hpp"
#endif
#ifdef WINE11_DEDICATED_HOST
#include "homebrew/artwork_backend.hpp"
#include "homebrew/title_icon.hpp"
#include "title_icon_fetch.hpp"
#include <atomic>
#endif
#include <utility>

namespace ww::steam_integration {
class NativeSession::Impl {
 public:
  Impl(NativePaths&& directories, const char* ca, bool persist)
      : paths(std::move(directories)), ca_bundle(ca), runtime(paths.state.path().string(), ca, persist) {}
  // Reverse destruction stops runtime workers before releasing the state lease.
  NativePaths paths;
  std::string ca_bundle;
  // Per-game controller layouts (Playden game settings). Declared before the
  // runtime so it outlives the UI adapter that refers to it.
  std::unique_ptr<ControllerLayoutStore> controls;
  std::unique_ptr<NativeGameRuntime> game_runtime;
  Runtime runtime;
};
NativeSession::NativeSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
NativeSession::~NativeSession() = default;
std::unique_ptr<NativeSession> NativeSession::Create(const char* root, int anchor, const char* ca, int game_data_anchor) {
  if (!ca || !*ca) return {};
  bool downloads_ready = game_data_anchor != -2;
  auto paths = game_data_anchor == -2 ? std::optional<NativePaths>{} : PrepareNativePaths(root, anchor, game_data_anchor);
  if (!paths && game_data_anchor != -1) {
    // Keep account/library access if the main-drive anchor is unavailable,
    // but never silently send game downloads back to the tiny app partition.
    paths = PrepareNativePaths(root, anchor);
    downloads_ready = false;
  }
  if (!paths) return {};
  const auto persist = HasSavedSteamSession(*paths);
  if (!persist) return {};
  auto impl = std::make_unique<Impl>(std::move(*paths), ca, *persist);
  if (downloads_ready && !impl->runtime.EnableDownloads(std::move(impl->paths.games)
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY) || defined(WINE11_EXPERIMENT_UNINSTALL)
      , impl->paths.state, impl->paths.state_lease
#endif
  )) return {};
  return std::unique_ptr<NativeSession>(new NativeSession(std::move(impl)));
}
ww::SteamUi& NativeSession::ui() { return impl_->runtime.ui(); }
bool NativeSession::AttachReadyLaunch(ReadyLaunch* launch) { return impl_->runtime.AttachReadyLaunch(launch); }
bool NativeSession::AttachPreparation(Preparation* preparation) { return impl_->runtime.AttachPreparation(preparation); }
CloudPreflight* NativeSession::EnableCloud() { return impl_->runtime.EnableCloud(); }
CloudLocationResolver NativeSession::CloudLocations(playden::homebrew::RuntimeHost& host) {
  using playden::platform::DirectoryPolicy;
  auto& paths = impl_->paths;
  if (!paths.state.Validate() || !paths.state_lease.Validate(paths.state, "wine11-state.lock")) return {};
  auto directory = paths.state.OpenChild("cloud-saves", DirectoryPolicy::kPrivate, true);
  if (!directory || !directory->Sync() || !paths.state.Sync()) return {};
  auto saves = std::make_shared<playden::platform::ManagedDirectory>(std::move(*directory));
  auto* state = &paths.state;
  auto* lease = &paths.state_lease;
  return [saves, state, lease, &host](std::uint32_t app) -> std::optional<playden::steam::CloudLocation> {
    if (!state->Validate() || !lease->Validate(*state, "wine11-state.lock") || !saves->Validate()) return {};
    auto snapshot = host.Shared();
    return snapshot ? CloudLocationFor(*snapshot, app, saves->path()) : std::nullopt;
  };
}
std::optional<playden::homebrew::RuntimeHostConfig> NativeSession::GameHostConfig(
    std::string dir, CoordinatedTitleList& titles, playden::ps5::TitleRegistry::Transport registry,
    NativeTitleLauncher launcher) {
  NativeHostInputs inputs{impl_->paths.state, impl_->paths.state_lease, std::move(dir), titles,
                          std::move(registry), InstalledKitAdmission(), std::move(launcher)};
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  inputs.unregister_title = MakeVaporTitleRemover(kVaporTitleRemoverPath);
#endif
#ifdef WINE11_DEDICATED_HOST
  // This build uses a private Wine 11 cache and the already verified CA.
  inputs.fetch_redist = MakeRedistFetch(impl_->ca_bundle);
#endif
  auto config = BuildNativeHostConfig(inputs);
  if (config) {
    if (!impl_->controls) {
      impl_->controls = std::make_unique<ControllerLayoutStore>(impl_->paths.state.path());
      (void)impl_->controls->Load();  // unreadable or malformed: Playden defaults apply
    }
    // RuntimeHost asks on its worker while preparing or applying settings.
    config->settings_record = [store = impl_->controls.get()](const playden::GameId& game)
        -> std::optional<playden::runtime::GameSettingsRecord> {
      const auto app = game.steam_app();
      return app ? store->Record(*app) : std::nullopt;
    };
  }
#ifdef WINE11_DEDICATED_HOST
  if (config) {
    // RuntimeHost invokes this while preparing title metadata on its worker.
    // Use only the selected game's canonical Steam cover and a verified CA.
    config->title_icon = [trust = impl_->ca_bundle](const playden::GameId& game) -> std::optional<std::string> {
      const auto app = game.steam_app();
      if (!app) return std::nullopt;
      playden::homebrew::HttpArtworkBackend backend(trust);
      const std::atomic<bool> cancel{false};
      // Some games have a hero but no library cover. Keep the fallback tied
      // to this exact app ID; never use another game's cached artwork.
      return FetchTitleIcon(*app, [&](const std::string& url) -> std::optional<std::string> {
        const auto cover = backend.Load({1, game, url}, cancel);
        return cover ? playden::homebrew::MakeTitleIcon(*cover) : std::nullopt;
      });
    };
  }
#endif
  return config;
}
bool NativeSession::AttachGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud) {
  if (impl_->game_runtime || !cloud) return false;
  auto game = std::make_unique<NativeGameRuntime>(host, std::move(cloud));
  if (!game->Attach(impl_->runtime)) return false;
  impl_->game_runtime = std::move(game);
  if (impl_->controls)
    (void)impl_->runtime.AttachControls(impl_->controls.get(), [&host](std::uint32_t app) {
      host.Maintain(app, playden::homebrew::RuntimeMaintenance::kApplySettings);
    });
  return true;
}
bool NativeSession::AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost& host, CloudLocationResolver resolver) {
  if (impl_->game_runtime || !resolver) return false;
  auto* cloud = impl_->runtime.EnableCloud();
  if (!cloud) return false;
  auto game = std::make_unique<NativeGameRuntime>(host, *cloud, std::move(resolver));
  if (!game->Attach(impl_->runtime)) return false;
  impl_->game_runtime = std::move(game);
  if (impl_->controls)
    (void)impl_->runtime.AttachControls(impl_->controls.get(), [&host](std::uint32_t app) {
      host.Maintain(app, playden::homebrew::RuntimeMaintenance::kApplySettings);
    });
  return true;
}
}
