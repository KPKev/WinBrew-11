// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_adapter.hpp"
#if defined(WINE11_EXPERIMENT_UNINSTALL)
#include "uninstall_flow.hpp"
#endif
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
extern "C" { [[gnu::used]] int wine11_cancel_cleanup_adapter_enabled = 1; }
#endif
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
extern "C" { [[gnu::used]] int wine11_cancel_guard_adapter_enabled = 1; }
#endif
#include <cstdio>
#include <chrono>
#include <cstring>
namespace ww::steam_integration {
UiAdapter::UiAdapter(Controller& controller) : controller_(controller) {}
bool UiAdapter::attach_downloads(DownloadCoordinator* downloads) {
  if (download_maintenance_) return false;
  if (downloads_ && downloads_ != downloads && pending_play_) return false;
  if (downloads_ && downloads_ != downloads && downloads_->Busy()) return false;
  downloads_ = downloads;
  download_snapshot_ = {};
  return true;
}
#if defined(WINE11_EXPERIMENT_UNINSTALL)
bool UiAdapter::attach_uninstall(UninstallFlow* flow) {
  if (!flow || uninstall_) return false;
  uninstall_ = flow; uninstall_error_.clear(); return true;
}
#endif
bool UiAdapter::detach_downloads_for_maintenance() {
  if (download_maintenance_ || !downloads_ || !downloads_->FrozenForMaintenance() || pending_play_ ||
      (preparation_ && preparation_->Busy()) || (launch_ && launch_->Busy()) ||
      (cloud_preflight_ && cloud_preflight_->Busy())) return false;
  downloads_ = nullptr; download_snapshot_ = {}; download_maintenance_ = true; return true;
}
bool UiAdapter::finish_download_maintenance(DownloadCoordinator* downloads) {
  if (!download_maintenance_ || !downloads || downloads_ || downloads->FrozenForMaintenance()) return false;
  downloads_ = downloads; download_snapshot_ = {}; download_maintenance_ = false; download_recovery_ = false; return true;
}
bool UiAdapter::begin_download_recovery() {
  if (downloads_ || download_maintenance_) return false;
  download_maintenance_ = true; download_recovery_ = true;
  message_ = "A previous game removal needs recovery before downloads can continue.";
  return true;
}
void UiAdapter::allow_download_recovery_signin() {
  if (download_maintenance_ && !downloads_) download_recovery_ = true;
}
bool UiAdapter::attach_ready_launch(ReadyLaunch* launch) {
  if (launch_ && launch_ != launch && pending_play_) return false;
  if (launch_ && launch_ != launch && launch_->Busy()) return false;
  launch_ = launch; return true;
}
bool UiAdapter::attach_preparation(Preparation* preparation) {
  if (preparation_ && preparation_ != preparation && pending_play_) return false;
  if (preparation_ && preparation_ != preparation && preparation_->Busy()) return false;
  preparation_ = preparation; return true;
}
bool UiAdapter::attach_game_runtime(ReadyLaunch* launch, Preparation* preparation) {
  if (!launch || !preparation || pending_play_) return false;
  if ((launch_ && launch_ != launch && launch_->Busy()) ||
      (preparation_ && preparation_ != preparation && preparation_->Busy())) return false;
  launch_ = launch; preparation_ = preparation; return true;
}
bool UiAdapter::attach_launch_details(LaunchDetails* details) {
  if (launch_details_ && launch_details_ != details && waiting_details_) return false;
  launch_details_ = details; return true;
}
bool UiAdapter::attach_cloud_preflight(CloudPreflight* cloud) {
  if (cloud_preflight_ && cloud_preflight_ != cloud && cloud_preflight_->Busy()) return false;
  cloud_preflight_ = cloud; return true;
}
bool UiAdapter::attach_remember(RememberSetting* setting) {
  if (remember_ && remember_ != setting && remember_->View().busy) return false;
  remember_ = setting; return true;
}
void UiAdapter::tick() {
  controller_.Tick();
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  if (uninstall_) {
#if !defined(WINE11_OWNER_INSPECT_ONLY)
    if (download_recovery_ && uninstall_->state() == UninstallFlowState::idle) (void)uninstall_->Recover();
#endif
    uninstall_->Tick();
  }
#endif
  if (game_details_) {
    const auto generation = controller_.view().account_generation;
    if (generation != game_details_generation_) {
      game_details_->Invalidate(generation); game_details_cache_.clear(); game_details_generation_ = generation;
    }
    for (auto& details : game_details_->Pump(8)) {
      const auto app = details.id.steam_app();
      if (app && details.account_generation == generation) game_details_cache_[*app] = std::move(details);
    }
  }
  if (cloud_preflight_) cloud_preflight_->Observe(controller_.view().account_id, controller_.view().account_generation);
  if (resolving_cloud_ && cloud_preflight_ && !cloud_preflight_->Busy()) {
    const auto result = cloud_preflight_->Poll(resolving_app_, controller_.view().account_id,
                                                controller_.view().account_generation);
    message_ = result.state == CloudGateState::ready ? "Cloud saves resolved. Press PLAY again."
      : (result.message.empty() ? "Cloud saves could not be resolved." : result.message);
    resolving_cloud_ = false;
  }
  if (downloads_) {
    download_snapshot_ = downloads_->Snapshot();
    network_app_ = 0;
    for (const auto& job : download_snapshot_.jobs)
      if (job.state == playden::installs::InstallJobState::kRunning) { network_app_ = job.id.steam_app().value_or(0); break; }
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    network_rate_ = network_sampler_.sample(downloads_->Traffic(network_app_), static_cast<std::uint64_t>(now));
  }
  if (waiting_details_ && launch_details_) {
    launch_details_->Tick(controller_.view().account_id, controller_.view().account_generation);
    if (launch_details_->state() == LaunchDetailsState::failed) {
      message_ = launch_details_->message(); waiting_details_ = false; pending_play_ = 0;
    } else if (launch_details_->state() == LaunchDetailsState::ready) {
      waiting_details_ = false;
      bool still_installed = false;
      for (const auto& game : download_snapshot_.installed)
        if (game.id.steam_app() == pending_play_ && game.available && game.root == pending_root_) still_installed = true;
      if (!preparation_ || !still_installed || !owns(pending_play_) || controller_.view().account_id != pending_account_ ||
          controller_.view().account_generation != pending_generation_) {
        message_ = "The account or game installation changed before preparation."; pending_play_ = 0;
      } else if (!preparation_->Request(launch_details_->request(), pending_account_, pending_generation_)) {
        message_ = preparation_->view().message; pending_play_ = 0;
      }
    }
  }
  if (preparation_) {
    preparation_->Tick(controller_.view().account_id, controller_.view().account_generation);
    const auto& prepared = preparation_->view();
    if (pending_play_ && !waiting_details_ && (prepared.state == PreparationState::failed || prepared.state == PreparationState::awaiting_registration)) {
      message_ = prepared.message; pending_play_ = 0;
    } else if (pending_play_ && !waiting_details_ && prepared.state == PreparationState::ready) {
      const auto app = pending_play_; pending_play_ = 0;
      bool still_installed = false;
      for (const auto& game : download_snapshot_.installed)
        if (game.id.steam_app() == app && game.available && game.root == pending_root_) still_installed = true;
      if (launch_ && still_installed && owns(app) && controller_.view().account_id == pending_account_ &&
          controller_.view().account_generation == pending_generation_) {
        if (!launch_->Request(app, pending_root_, pending_account_, pending_generation_))
          message_ = launch_->view().message;
      } else message_ = "The account or game installation changed before the game could start.";
    }
  }
  if (launch_) launch_->Tick(controller_.view().account_id, controller_.view().account_generation);
#if defined(WINE11_OWNER_INSPECT_ONLY) && defined(WINE11_EXPERIMENT_UNINSTALL)
  if (owner_blender_inspection_ && owner_blender_inspection_()) {
    owner_blender_inspection_ = {};
    message_ = "Blender removal status checked.";
  }
#endif
#if defined(WINE11_OWNER_BLENDER_ONESHOT) && defined(WINE11_EXPERIMENT_UNINSTALL)
  // Explicitly armed only in the temporary owner build, after the exact target
  // and idle state are known. A claimed request is never automatically retried.
  if (owner_blender_claim_ && uninstall_ && controller_.view().account == AccountState::online &&
      controller_.view().library == LibraryState::ready) {
    const auto& games = controller_.view().games;
    for (std::size_t i = 0; i < games.size(); ++i) {
      if (games[i].id.steam_app() != 365670) continue;
      SteamGame target;
      const auto title = uninstall_->TitleFor(365670);
      if (!game(i, target) || !target.can_manage_install || !title || *title != "FAKE78008") break;
      const auto claim = owner_blender_claim_();
      owner_blender_claim_ = {};
      if (claim == OwnerBlenderClaim::claimed) {
        if (!remove_or_reinstall(365670, false) && message_.empty())
          message_ = "Blender removal could not start.";
      } else if (claim == OwnerBlenderClaim::rejected) {
        message_ = "Blender removal request could not be verified.";
      }
      break;
    }
  }
#endif
  const auto account = controller_.view().account;
  if (account == AccountState::online || account == AccountState::offline) account_session_seen_ = true;
  else if (account == AccountState::signed_out) account_session_seen_ = false;
  const auto& url = controller_.view().challenge_url;
  if (url == encoded_url_) return;
  encoded_url_ = url;
  std::memset(qr_, 0, sizeof(qr_));
  qr_size_ = 0;
  if (!url.empty()) {
    std::uint8_t scratch[qrcodegen_BUFFER_LEN_MAX]{};
    if (qrcodegen_encodeText(url.c_str(), scratch, qr_, qrcodegen_Ecc_MEDIUM,
                             qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                             qrcodegen_Mask_AUTO, true))
      qr_size_ = qrcodegen_getSize(qr_);
    std::memset(scratch, 0, sizeof(scratch));
  }
}
SteamStatus UiAdapter::status() const {
  const auto& view = controller_.view(); SteamStatus out;
  switch (view.account) {
  case AccountState::signed_out: out.account = SteamAccount::signed_out; break;
  case AccountState::signing_in: out.account = SteamAccount::signing_in; break;
  case AccountState::approval: out.account = SteamAccount::approval; break;
  case AccountState::online: out.account = SteamAccount::online; break;
  case AccountState::offline: out.account = SteamAccount::offline; break;
  case AccountState::signing_out: out.account = SteamAccount::signing_out; break;
  case AccountState::failed: out.account = SteamAccount::failed; break;
  default: out.account = SteamAccount::failed; break;
  }
  switch (view.library) {
  case LibraryState::empty: out.library = SteamLibrary::empty; break;
  case LibraryState::loading: out.library = SteamLibrary::loading; break;
  case LibraryState::ready: out.library = SteamLibrary::ready; break;
  case LibraryState::failed: out.library = SteamLibrary::failed; break;
  }
  std::snprintf(out.account_name, sizeof(out.account_name), "%s", view.account_name.c_str());
  if (view.account == AccountState::failed)
    std::snprintf(out.message, sizeof(out.message), "Steam sign-in failed (error %d).", int(view.auth_error));
  else if (view.library == LibraryState::failed) {
    if (view.library_error == playden::steam::LibraryError::kNetwork)
      std::snprintf(out.message, sizeof(out.message), "Steam connection failed. Showing cached games; try Refresh library.");
    else
      std::snprintf(out.message, sizeof(out.message), "Steam library could not be loaded (error %d).", int(view.library_error));
  }
  if (!message_.empty()) std::snprintf(out.message, sizeof(out.message), "%s", message_.c_str());
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  if (uninstall_ && uninstall_->state() != UninstallFlowState::idle && !uninstall_->message().empty())
    std::snprintf(out.message, sizeof(out.message), "%s", uninstall_->message().c_str());
#endif
  if (!out.message[0] && downloads_) {
    const auto error = downloads_->Error();
    if (!error.empty()) std::snprintf(out.message, sizeof(out.message), "%s", error.c_str());
  }
  if (!out.message[0] && preparation_ && preparation_->Busy())
    std::snprintf(out.message, sizeof(out.message), "%s", preparation_->view().message.c_str());
  if (!out.message[0] && waiting_details_ && launch_details_)
    std::snprintf(out.message, sizeof(out.message), "%s", launch_details_->message().c_str());
  if (!out.message[0] && launch_ && !launch_->view().message.empty())
    std::snprintf(out.message, sizeof(out.message), "%s", launch_->view().message.c_str());
  out.game_count = view.games.size(); out.qr_size = qr_size_;
  out.downloads_available = downloads_ != nullptr;
  out.download_count = download_snapshot_.jobs.size() + download_snapshot_.completed.size();
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  out.download_count += download_snapshot_.cancel_cleanup_failures.size();
#endif
  if (remember_) {
    const auto remembered = remember_->View();
    out.remember_available = true; out.remember_enabled = remembered.enabled; out.remember_busy = remembered.busy;
    if (remembered.failed) std::snprintf(out.message, sizeof(out.message), "Could not change saved sign-in. Try again.");
  }
  if (cloud_preflight_) {
    const auto app = cloud_preflight_->ChoiceApp(view.account_id, view.account_generation);
    if (cloud_choice_current(app)) out.cloud_choice_app = app;
    const auto skip = cloud_preflight_->SkipApp(view.account_id, view.account_generation);
    if (launch_ && cloud_choice_current(skip)) out.cloud_skip_app = skip;
  }
  return out;
}
void UiAdapter::request_artwork(std::uint32_t app) {
  if (!owns(app)) return;
  if (artwork_) artwork_->Request(app);
  if (game_details_ && game_details_generation_) game_details_->Request(playden::GameId::Steam(app), game_details_generation_);
}
bool UiAdapter::take_artwork(std::uint32_t app, SteamArtwork& artwork) {
  return artwork_ && owns(app) && artwork_->Take(app, artwork);
}
void UiAdapter::request_game_icon(std::uint32_t app) {
  if (!artwork_ || !owns(app)) return;
  for (const auto& game : controller_.view().games)
    if (game.id.steam_app() == app) { artwork_->RequestIcon(app, game.artwork_source); return; }
}
bool UiAdapter::take_game_icon(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  return artwork_ && owns(app) && artwork_->TakeIcon(app, bytes);
}
void UiAdapter::reject_game_icon(std::uint32_t app) {
  if (artwork_ && owns(app)) artwork_->RejectIcon(app);
}
void UiAdapter::request_game_cover(std::uint32_t app) {
  if (artwork_ && owns(app)) artwork_->RequestCover(app);
}
bool UiAdapter::take_game_cover(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  return artwork_ && owns(app) && artwork_->TakeCover(app, bytes);
}
void UiAdapter::request_game_portrait(std::uint32_t app) {
  if (artwork_ && owns(app)) artwork_->RequestPortrait(app);
}
bool UiAdapter::take_game_portrait(std::uint32_t app, std::vector<std::uint8_t>& bytes) {
  return artwork_ && owns(app) && artwork_->TakePortrait(app, bytes);
}
void UiAdapter::reject_game_portrait(std::uint32_t app) {
  if (artwork_ && owns(app)) artwork_->RejectPortrait(app);
}
void UiAdapter::reject_game_cover(std::uint32_t app) {
  if (artwork_ && owns(app)) artwork_->RejectCover(app);
}
bool UiAdapter::finish_setup(std::uint32_t app) {
  if (download_maintenance_) { message_ = "Wait for game removal to finish."; return false; }
  if (!owns(app) || !preparation_ || !preparation_->NeedsRegistration(app)) {
    message_ = "This game is no longer waiting for registration."; return false;
  }
  if (pending_play_ || (preparation_ && preparation_->Busy()) || (launch_ && launch_->Busy()) ||
      (cloud_preflight_ && cloud_preflight_->Busy()) || (downloads_ && !downloads_->SafeToExit())) {
    message_ = "Pause active downloads and wait for game operations before returning to PS5 Home."; return false;
  }
  return true;
}
bool UiAdapter::game(std::size_t index, SteamGame& out) const {
  const auto& games = controller_.view().games;
  if (index >= games.size()) return false;
  const auto app = games[index].id.steam_app();
  if (!app) return false;
  out = SteamGame{};
  out.app_id = *app;
  if (downloads_) {
    out.install = SteamInstallState::not_installed;
    for (const auto& installed : download_snapshot_.installed)
      if (installed.id == games[index].id)
        out.install = installed.available ? SteamInstallState::installed : SteamInstallState::unavailable;
    for (const auto& job : download_snapshot_.jobs) if (job.id == games[index].id) {
      std::snprintf(out.detail, sizeof(out.detail), "%s", job.failure.c_str());
      switch (job.state) {
      case playden::installs::InstallJobState::kQueued: out.install = SteamInstallState::queued; break;
      case playden::installs::InstallJobState::kRunning: out.install = SteamInstallState::running; break;
      case playden::installs::InstallJobState::kPaused: out.install = SteamInstallState::paused; break;
      case playden::installs::InstallJobState::kFailed: out.install = SteamInstallState::failed; break;
      }
    }
  }
  out.can_play = launch_ && out.install == SteamInstallState::installed && owns(*app);
  if (preparation_ && preparation_->NeedsRegistration(*app)) out.activity = SteamGameActivity::registration_required;
  else if (pending_play_ == *app || (preparation_ && preparation_->Busy() && preparation_->app() == *app)) out.activity = SteamGameActivity::preparing;
  else if (launch_ && launch_->view().app == *app) {
    const auto state = launch_->view().state;
    if (state == ReadyLaunchState::running) out.activity = SteamGameActivity::playing;
    else if (state == ReadyLaunchState::waiting || state == ReadyLaunchState::handoff) out.activity = SteamGameActivity::starting;
  }
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  out.can_manage_install = uninstall_ && downloads_ && out.install == SteamInstallState::installed &&
      out.activity == SteamGameActivity::idle && downloads_->MaintenanceReady() && !download_maintenance_ && owns(*app) &&
      !pending_play_ && !(preparation_ && preparation_->Busy()) && !(launch_ && launch_->Busy()) &&
      !(cloud_preflight_ && cloud_preflight_->Busy()) && uninstall_->TitleFor(*app).has_value();
  out.can_reinstall = out.can_manage_install && controller_.view().account == AccountState::online &&
      controller_.view().library == LibraryState::ready;
  if (out.install == SteamInstallState::installed && !out.can_manage_install) {
    const char* why = !uninstall_ ? uninstall_error_.c_str() :
        (!downloads_ || download_maintenance_) ? "Removal is waiting for recovery" :
        !downloads_->MaintenanceReady() ? "Pause active downloads before removal" :
        (out.activity != SteamGameActivity::idle || pending_play_ ||
         (preparation_ && preparation_->Busy()) || (launch_ && launch_->Busy()) ||
         (cloud_preflight_ && cloud_preflight_->Busy())) ? "Wait for game activity to finish" :
        !owns(*app) ? "Sign in to manage this game" : "PS5 game entry is not ready for removal";
    std::snprintf(out.management_reason, sizeof(out.management_reason), "%s", why);
  }
#endif
  if (games[index].playtime_minutes) {
    out.has_playtime = true; out.playtime_minutes = *games[index].playtime_minutes;
  }
  if (games[index].last_played_unix_seconds) {
    out.has_last_played = true; out.last_played_unix_seconds = *games[index].last_played_unix_seconds;
  }
  const auto info = game_details_cache_.find(*app);
  if (info != game_details_cache_.end() && info->second.available) {
    if (info->second.install_bytes) { out.has_install_bytes = true; out.install_bytes = *info->second.install_bytes; }
    if (info->second.download_bytes) { out.has_download_bytes = true; out.download_bytes = *info->second.download_bytes; }
  }
  std::snprintf(out.title, sizeof(out.title), "%s", games[index].title.c_str());
  return true;
}
bool UiAdapter::qr_module(int x, int y) const {
  return qr_size_ > 0 && x >= 0 && y >= 0 && x < qr_size_ && y < qr_size_ && qrcodegen_getModule(qr_, x, y);
}
void UiAdapter::sign_in() {
  if (download_maintenance_ && !download_recovery_) { message_ = "Wait for game removal to finish before changing account."; return; }
  if (remember_ && (remember_->View().busy || remember_->View().failed)) {
    message_ = "Finish setting saved sign-in before signing in."; return;
  }
  if (cloud_preflight_ && cloud_preflight_->Busy()) { message_ = "Wait for Cloud saves to finish before changing account."; return; }
  if (preparation_ && preparation_->Busy()) { message_ = "Wait for the game to finish preparing before changing account."; return; }
  if (launch_ && launch_->Busy()) { message_ = "Wait for the game session to finish before changing account."; return; }
  // Restored queued jobs may need a first login after a restart. Changing an
  // already active account remains blocked while any download work exists.
  const auto account = controller_.view().account;
  const bool first_login = !account_session_seen_ &&
      (account == AccountState::signed_out || account == AccountState::failed);
  if (downloads_ && downloads_->Busy() && !first_login) {
    message_ = downloads_->Error();
    if (message_.empty()) message_ = "Finish or cancel downloads before changing account.";
    return;
  }
  if (waiting_details_ && launch_details_) { launch_details_->Cancel(); waiting_details_ = false; pending_play_ = 0; }
  message_.clear(); controller_.SignInQr(); tick();
}
void UiAdapter::cancel() { controller_.Cancel(); tick(); }
bool UiAdapter::set_remember(bool enabled) {
  const auto account = controller_.view().account;
  if (!remember_ || (account != AccountState::signed_out && account != AccountState::failed)) return false;
  message_.clear();
  return remember_->Set(enabled);
}
void UiAdapter::sign_out() {
  if (download_maintenance_ && !download_recovery_) { message_ = "Wait for game removal to finish before signing out."; return; }
  if (cloud_preflight_ && cloud_preflight_->Busy()) { message_ = "Wait for Cloud saves to finish before signing out."; return; }
  if (preparation_ && preparation_->Busy()) { message_ = "Wait for the game to finish preparing before signing out."; return; }
  if (launch_ && launch_->Busy()) { message_ = "Wait for the game session to finish before signing out."; return; }
  if (controller_.view().account == AccountState::online || controller_.view().account == AccountState::offline)
    account_session_seen_ = true;
  if (downloads_ && downloads_->Busy()) {
    message_ = downloads_->Error();
    if (message_.empty()) message_ = "Finish or cancel downloads before signing out.";
    return;
  }
  if (waiting_details_ && launch_details_) { launch_details_->Cancel(); waiting_details_ = false; pending_play_ = 0; }
  message_.clear(); controller_.SignOut(); tick();
}
void UiAdapter::refresh() {
  message_.clear();
  const auto account = controller_.view().account;
  if (game_details_ && (account == AccountState::online || account == AccountState::offline)) {
    // A transient unavailable response may be deduplicated for this account.
    // Explicit refresh must allow new details requests as well as library data.
    game_details_->Invalidate(controller_.view().account_generation);
    game_details_cache_.clear();
  }
  controller_.Refresh(); tick();
}
bool UiAdapter::owns(std::uint32_t app) const {
  if (!app || controller_.view().account != AccountState::online) return false;
  for (const auto& game : controller_.view().games)
    if (game.id.steam_app() == app) return true;
  return false;
}
bool UiAdapter::cloud_choice_current(std::uint32_t app) const {
  if (!app || !cloud_preflight_ || !downloads_ || !owns(app) ||
      (launch_ && launch_->Busy()) || (preparation_ && preparation_->Busy()) || pending_play_) return false;
  for (const auto& game : download_snapshot_.installed)
    if (game.id.steam_app() == app && game.available && game.root == cloud_preflight_->ChoiceGameRoot()) return true;
  return false;
}
bool UiAdapter::download(std::size_t index, SteamDownload& out) const {
  if (!downloads_) return false;
  out = SteamDownload{};
  if (index >= download_snapshot_.jobs.size()) {
    index -= download_snapshot_.jobs.size();
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
    if (index < download_snapshot_.cancel_cleanup_failures.size()) {
      const auto& failure = download_snapshot_.cancel_cleanup_failures[index];
      const auto app = failure.id.steam_app();
      if (!app) return false;
      out.app_id = *app; out.state = SteamInstallState::failed; out.cleanup_failure = true;
      for (const auto& id : download_snapshot_.cancel_cleanup_retryable)
        if (id == failure.id) out.cleanup_retryable = true;
      std::snprintf(out.title, sizeof(out.title), "Steam game %u", *app);
      for (const auto& game : controller_.view().games) if (game.id == failure.id) {
        std::snprintf(out.title, sizeof(out.title), "%s", game.title.c_str()); break;
      }
      std::snprintf(out.detail, sizeof(out.detail), "%s", failure.reason.c_str());
      return true;
    }
    index -= download_snapshot_.cancel_cleanup_failures.size();
#endif
    if (index >= download_snapshot_.completed.size()) return false;
    const auto& complete = download_snapshot_.completed[index]; auto app = complete.id.steam_app();
    if (!app) return false;
    // CompletedDownload records installed disk bytes, not network transfer
    // bytes. The completed transfer total is unavailable; leave it unknown.
    out.app_id = *app; out.state = SteamInstallState::installed;
    std::snprintf(out.title, sizeof(out.title), "Steam game %u", *app);
    for (const auto& game : controller_.view().games) if (game.id == complete.id) {
      std::snprintf(out.title, sizeof(out.title), "%s", game.title.c_str()); break;
    }
    return true;
  }
  const auto& job = download_snapshot_.jobs[index]; auto app = job.id.steam_app();
  if (!app) return false;
  out.app_id = *app; out.done = job.done_bytes; out.total = job.total_bytes;
  if (network_app_ == *app && network_rate_) { out.has_rate = true; out.bytes_per_second = *network_rate_; }
  if (job.eta_seconds && *job.eta_seconds >= 0) { out.has_eta = true; out.eta_seconds = *job.eta_seconds; }
  switch (job.state) {
  case playden::installs::InstallJobState::kQueued: out.state = SteamInstallState::queued; break;
  case playden::installs::InstallJobState::kRunning: out.state = SteamInstallState::running; break;
  case playden::installs::InstallJobState::kPaused: out.state = SteamInstallState::paused; break;
  case playden::installs::InstallJobState::kFailed: out.state = SteamInstallState::failed; break;
  }
  std::snprintf(out.title, sizeof(out.title), "Steam game %u", *app);
  for (const auto& game : controller_.view().games) if (game.id == job.id) {
    std::snprintf(out.title, sizeof(out.title), "%s", game.title.c_str()); break;
  }
  std::snprintf(out.detail, sizeof(out.detail), "%s", job.failure.c_str());
  return true;
}
bool UiAdapter::install(std::uint32_t app) {
  message_.clear();
  if (!downloads_ || !owns(app)) return false;
  if (!downloads_->Install(app)) { message_ = "Could not queue the download."; return false; }
  return true;
}
#if defined(WINE11_EXPERIMENT_UNINSTALL)
bool UiAdapter::remove_or_reinstall(std::uint32_t app, bool reinstall) {
  if (!uninstall_ || !downloads_ || !downloads_->MaintenanceReady() || download_maintenance_ || !owns(app) || pending_play_ ||
      (preparation_ && preparation_->Busy()) || (launch_ && launch_->Busy()) || (cloud_preflight_ && cloud_preflight_->Busy())) return false;
  if (reinstall && (controller_.view().account != AccountState::online || controller_.view().library != LibraryState::ready)) return false;
  const auto title = uninstall_->TitleFor(app);
  if (!title) return false;
  message_.clear();
  const bool started = uninstall_->Begin(app, *title, reinstall, controller_.view().account_id);
  if (!started && !uninstall_->message().empty()) message_ = uninstall_->message();
  return started;
}
#endif
bool UiAdapter::has_job(std::uint32_t app) const {
  for (const auto& job : download_snapshot_.jobs) if (job.id.steam_app() == app) return true;
  return false;
}
bool UiAdapter::pause_download(std::uint32_t app) { return downloads_ && has_job(app) && downloads_->Pause(app); }
bool UiAdapter::resume_download(std::uint32_t app) { return downloads_ && owns(app) && has_job(app) && downloads_->Resume(app); }
bool UiAdapter::can_cancel_download() const {
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
  return false;
#else
  return true;
#endif
}
bool UiAdapter::cancel_download(std::uint32_t app) {
  if (!downloads_) return false;
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  for (const auto& id : download_snapshot_.cancel_cleanup_retryable)
    if (id.steam_app() == app) return downloads_->Cancel(app);
#endif
  return has_job(app) && downloads_->Cancel(app);
}
bool UiAdapter::play(std::uint32_t app) {
  message_.clear();
  if (cloud_preflight_ && cloud_preflight_->Busy()) {
    message_ = "Wait for Cloud saves to finish before starting a game."; return false;
  }
  if (!launch_ || !downloads_ || !owns(app)) return false;
  for (const auto& game : download_snapshot_.installed) {
    if (game.id.steam_app() == app && game.available) {
      if (!preparation_)
        return launch_->Request(app, game.root, controller_.view().account_id, controller_.view().account_generation);
      if (preparation_->Busy() || launch_->Busy() || pending_play_) return false;
      playden::homebrew::RuntimePrepareRequest request;
      request.runtime_number = app; request.game = game.id; request.install_path = game.root; request.volume_id = game.volume_id;
      for (const auto& owned : controller_.view().games)
        if (owned.id == game.id) { request.title_name = owned.title; break; }
      const auto account = controller_.view().account_id, generation = controller_.view().account_generation;
      pending_play_ = app; pending_root_ = game.root; pending_account_ = account; pending_generation_ = generation;
      if (launch_details_) {
        if (!launch_details_->Request(std::move(request), account, generation)) { pending_play_ = 0; return false; }
        waiting_details_ = true;
      } else {
        request.launch_reason = "Steam launch options are not available yet; a game program may need to be selected.";
        if (!preparation_->Request(std::move(request), account, generation)) {
          message_ = preparation_->view().message; pending_play_ = 0; return false;
        }
      }
      tick(); return true;
    }
  }
  message_ = "The installed game is unavailable."; return false;
}
bool UiAdapter::resolve_cloud(std::uint32_t app, bool use_cloud) {
  if (!cloud_choice_current(app) || cloud_preflight_->ChoiceApp(controller_.view().account_id,
      controller_.view().account_generation) != app) return false;
  const auto choice = use_cloud ? playden::steam::CloudResolution::kUseCloud : playden::steam::CloudResolution::kUseLocal;
  if (!cloud_preflight_->Resolve(app, controller_.view().account_id, controller_.view().account_generation, choice, true)) {
    message_ = "Cloud saves changed. Check them again before choosing."; return false;
  }
  resolving_cloud_ = true; resolving_app_ = app;
  message_ = "Resolving saves. When finished, press PLAY again.";
  return true;
}
bool UiAdapter::play_without_cloud(std::uint32_t app) {
  if (!launch_ || !cloud_choice_current(app) ||
      cloud_preflight_->SkipApp(controller_.view().account_id, controller_.view().account_generation) != app) return false;
  const auto root = cloud_preflight_->ChoiceGameRoot();
  const auto account = controller_.view().account_id, generation = controller_.view().account_generation;
  if (!cloud_preflight_->AllowWithoutCloud(app, account, generation)) return false;
  message_.clear();
  if (!launch_->RequestUsingCompletedCloud(app, root, account, generation)) {
    message_ = launch_->view().message; return false;
  }
  return true;
}
bool UiAdapter::dismiss_download(std::uint32_t app) {
  if (!downloads_) return false;
  for (const auto& complete : download_snapshot_.completed)
    if (complete.id.steam_app() == app) return downloads_->DismissCompleted(app);
  return false;
}
bool UiAdapter::attach_controls(ControllerLayoutStore* store, std::function<void(std::uint32_t)> apply) {
  if (controls_ || !store || !apply) return false;
  controls_ = store; apply_controls_ = std::move(apply); return true;
}
bool UiAdapter::controls(std::uint32_t app, SteamControls& out) const {
  if (!controls_ || !owns(app)) return false;
  out = static_cast<SteamControls>(static_cast<int>(controls_->Get(app)));
  return true;
}
bool UiAdapter::set_controls(std::uint32_t app, SteamControls choice) {
  message_.clear();
  if (!controls_ || !owns(app)) return false;
  if (pending_play_ == app || (launch_ && launch_->Busy())) {
    message_ = "Wait for the game to finish starting before changing its controller layout."; return false;
  }
  const auto layout = static_cast<ControllerLayout>(static_cast<int>(choice));
  if (!controls_->Set(app, layout)) { message_ = "Could not save the controller layout."; return false; }
  // Playden publishes the settings into the installed title while it is stopped;
  // a game that is not prepared yet picks them up when it is prepared.
  if (apply_controls_) apply_controls_(app);
  // The CONTROLS button itself shows the new layout; no status line needed.
  return true;
}
}
