// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../steam_ui.hpp"
#include "controller.hpp"
#include "downloads.hpp"
#include "ready_launch.hpp"
#include "preparation.hpp"
#include "launch_details.hpp"
#include "cloud_preflight.hpp"
#include "remember_setting.hpp"
#include "artwork.hpp"
#include "controller_layouts.hpp"
#include <functional>
#include <utility>
#include "qrcodegen.h"
#include <map>
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
#include "owner_blender_request.hpp"
#endif
namespace ww::steam_integration {
class UninstallFlow;
// Adapts Playden's asynchronous controller to the renderer's bounded interface.
class UiAdapter final : public SteamUi {
 public:
  explicit UiAdapter(Controller&);
  void tick() override;
  SteamStatus status() const override;
  bool game(std::size_t, SteamGame&) const override;
  void request_artwork(std::uint32_t) override;
  bool take_artwork(std::uint32_t, SteamArtwork&) override;
  void request_game_icon(std::uint32_t) override;
  bool take_game_icon(std::uint32_t, std::vector<std::uint8_t>&) override;
  void reject_game_icon(std::uint32_t) override;
  void request_game_cover(std::uint32_t) override;
  bool take_game_cover(std::uint32_t, std::vector<std::uint8_t>&) override;
  void reject_game_cover(std::uint32_t) override;
  void request_game_portrait(std::uint32_t) override;
  bool take_game_portrait(std::uint32_t, std::vector<std::uint8_t>&) override;
  void reject_game_portrait(std::uint32_t) override;
  void attach_artwork(ArtworkSource* source) { artwork_ = source; }
  void attach_game_details(playden::steam::AppDetailsService* service) { game_details_ = service; }
  bool qr_module(int, int) const override;
  void sign_in() override;
  void cancel() override;
  void sign_out() override;
  void refresh() override;
  // Coordinator and its exclusive service must outlive this adapter.
  bool attach_downloads(DownloadCoordinator* downloads);
  // Only a successfully frozen coordinator can be detached for the
  // journaled registry transaction; account changes remain blocked.
  bool detach_downloads_for_maintenance();
  bool finish_download_maintenance(DownloadCoordinator* downloads);
  bool begin_download_recovery();
  void allow_download_recovery_signin();
  bool downloads_attached() const { return downloads_ != nullptr; }
  bool download(std::size_t, SteamDownload&) const override;
  bool install(std::uint32_t) override;
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  bool attach_uninstall(UninstallFlow* flow);
  void uninstall_unavailable(std::string reason) { uninstall_error_ = std::move(reason); }
  bool remove_or_reinstall(std::uint32_t app, bool reinstall) override;
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
  void arm_owner_blender_request(std::function<OwnerBlenderClaim()> claim) { owner_blender_claim_ = std::move(claim); }
#endif
#if defined(WINE11_OWNER_INSPECT_ONLY)
  void arm_owner_blender_inspection(std::function<bool()> inspect) { owner_blender_inspection_ = std::move(inspect); }
#endif
#endif
  bool pause_download(std::uint32_t) override;
  bool resume_download(std::uint32_t) override;
  bool cancel_download(std::uint32_t) override;
  bool can_cancel_download() const override;
  bool dismiss_download(std::uint32_t) override;
  // The host and cloud gate must outlive this adapter, and the platform must
  // construct and tick them on the application thread.
  bool attach_ready_launch(ReadyLaunch*);
  bool attach_preparation(Preparation*);
  bool attach_game_runtime(ReadyLaunch*, Preparation*);
  bool attach_launch_details(LaunchDetails*);
  bool attach_cloud_preflight(CloudPreflight*);
  bool play(std::uint32_t) override;
  bool finish_setup(std::uint32_t) override;
  bool resolve_cloud(std::uint32_t, bool use_cloud) override;
  bool play_without_cloud(std::uint32_t) override;
  bool attach_remember(RememberSetting* setting);
  bool set_remember(bool enabled) override;
  // The store and apply callback must outlive this adapter. apply asks
  // Playden to publish the game's saved settings (kApplySettings).
  bool attach_controls(ControllerLayoutStore* store, std::function<void(std::uint32_t)> apply);
  bool controls(std::uint32_t, SteamControls&) const override;
  bool set_controls(std::uint32_t, SteamControls) override;
 private:
  bool owns(std::uint32_t) const;
  bool cloud_choice_current(std::uint32_t) const;
  bool has_job(std::uint32_t) const;
  Controller& controller_;
  ArtworkSource* artwork_ = nullptr;
  playden::steam::AppDetailsService* game_details_ = nullptr;
  std::uint64_t game_details_generation_ = 0;
  std::map<std::uint32_t, playden::steam::AppDetails> game_details_cache_;
  DownloadCoordinator* downloads_ = nullptr;
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  UninstallFlow* uninstall_ = nullptr;
  std::string uninstall_error_ = "Removal host not connected";
#if defined(WINE11_OWNER_BLENDER_ONESHOT)
  std::function<OwnerBlenderClaim()> owner_blender_claim_;
#endif
#if defined(WINE11_OWNER_INSPECT_ONLY)
  std::function<bool()> owner_blender_inspection_;
#endif
#endif
  bool download_maintenance_ = false;
  bool download_recovery_ = false;
  ReadyLaunch* launch_ = nullptr;
  Preparation* preparation_ = nullptr;
  LaunchDetails* launch_details_ = nullptr;
  CloudPreflight* cloud_preflight_ = nullptr;
  RememberSetting* remember_ = nullptr;
  ControllerLayoutStore* controls_ = nullptr;
  std::function<void(std::uint32_t)> apply_controls_;
  bool waiting_details_ = false;
  bool resolving_cloud_ = false;
  std::uint32_t resolving_app_ = 0;
  std::uint32_t pending_play_ = 0;
  std::uint64_t pending_account_ = 0, pending_generation_ = 0;
  std::string pending_root_;
  std::string message_;
  playden::installs::InstallSnapshot download_snapshot_;
  traffic::Sampler network_sampler_;
  std::uint32_t network_app_ = 0;
  std::optional<std::uint64_t> network_rate_;
  std::string encoded_url_;
  std::uint8_t qr_[qrcodegen_BUFFER_LEN_MAX]{};
  int qr_size_ = 0;
  bool account_session_seen_ = false;
};
}
