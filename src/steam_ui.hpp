// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ww {
// Interface owned by the platform. Rendering never reads credentials or private URLs.
enum class SteamAccount { signed_out, signing_in, approval, online, offline, signing_out, failed };
enum class SteamLibrary { empty, loading, ready, failed };
enum class SteamInstallState { unknown, not_installed, queued, running, paused, failed, installed, unavailable };
enum class SteamGameActivity { idle, preparing, registration_required, starting, playing };
struct SteamStatus {
  SteamAccount account = SteamAccount::signed_out;
  SteamLibrary library = SteamLibrary::empty;
  char account_name[96]{}, message[160]{};
  std::size_t game_count = 0;
  int qr_size = 0;
  bool downloads_available = false;
  std::size_t download_count = 0;
  bool remember_available = false, remember_enabled = false, remember_busy = false;
  std::uint32_t cloud_choice_app = 0;
  std::uint32_t cloud_skip_app = 0;
};
struct SteamGame {
  std::uint32_t app_id = 0; char title[192]{}, detail[160]{};
  SteamInstallState install = SteamInstallState::unknown; bool can_play = false;
  bool can_manage_install = false;
  bool can_reinstall = false;
  char management_reason[128]{};
  SteamGameActivity activity = SteamGameActivity::idle;
  bool has_playtime = false, has_last_played = false;
  std::uint64_t playtime_minutes = 0, last_played_unix_seconds = 0;
  bool has_install_bytes = false, has_download_bytes = false;
  std::uint64_t install_bytes = 0, download_bytes = 0;
};
enum class SteamNewsState { pending, available, empty, unavailable };
// Per-game controller layout (Playden game settings, applied to the installed title).
enum class SteamControls { controller, controller_pointer, mouse, arrows };
inline const char *steam_controls_name(SteamControls c) {
  switch (c) {
    case SteamControls::controller: return "Controller";
    case SteamControls::controller_pointer: return "Controller + pointer";
    case SteamControls::mouse: return "Mouse";
    case SteamControls::arrows: return "Arrow keys";
  }
  return "Controller";
}
inline const char *steam_controls_hint(SteamControls c) {
  switch (c) {
    case SteamControls::controller: return "Xbox layout, for games with controller support.";
    case SteamControls::controller_pointer: return "Xbox layout; right stick moves the pointer, touchpad click clicks.";
    case SteamControls::mouse: return "Left stick is the mouse; Cross clicks, Circle right-clicks.";
    case SteamControls::arrows: return "D-pad arrows; Cross Z, Circle X, Triangle C, Options Enter.";
  }
  return "";
}
inline SteamControls steam_controls_next(SteamControls c) { return SteamControls((int(c) + 1) % 4); }
struct SteamArtwork {
  std::vector<std::uint8_t> hero, logo;
  std::string description, genres;
  SteamNewsState news_state = SteamNewsState::pending;
  std::string news_title, news_feed, news_description;
  std::uint64_t news_published = 0;
};
struct SteamDownload {
  std::uint32_t app_id = 0;
  SteamInstallState state = SteamInstallState::unknown;
  std::uint64_t done = 0, total = 0;
  bool has_rate = false, has_eta = false;
  bool cleanup_failure = false, cleanup_retryable = false;
  std::uint64_t bytes_per_second = 0;
  int eta_seconds = 0;
  char title[192]{}, detail[160]{};
};
class SteamUi {
 public:
  virtual ~SteamUi() = default;
  virtual void tick() = 0;
  virtual SteamStatus status() const = 0;
  virtual bool game(std::size_t index, SteamGame&) const = 0;
  // Lightweight display fields; no runtime admission/management checks.
  virtual bool game_summary(std::size_t index, SteamGame& out) const { return game(index, out); }
  virtual bool reset_game_cache(std::uint32_t) { return false; }
  // Providers may bound background library scans while input remains frame-paced.
  virtual std::uint32_t library_poll_interval_ms() const { return 0; }
  // Game artwork is public, optional, and fetched asynchronously.
  virtual void request_artwork(std::uint32_t) {}
  virtual bool take_artwork(std::uint32_t, SteamArtwork&) { return false; }
  virtual void request_game_icon(std::uint32_t) {}
  virtual bool take_game_icon(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void reject_game_icon(std::uint32_t) {}
  virtual void request_game_cover(std::uint32_t) {}
  virtual bool take_game_cover(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void reject_game_cover(std::uint32_t) {}
  virtual void request_game_portrait(std::uint32_t) {}
  virtual bool take_game_portrait(std::uint32_t, std::vector<std::uint8_t>&) { return false; }
  virtual void reject_game_portrait(std::uint32_t) {}
  virtual bool qr_module(int x, int y) const = 0;
  virtual void sign_in() = 0;
  virtual void cancel() = 0;
  virtual void sign_out() = 0;
  virtual void refresh() = 0;
  virtual bool download(std::size_t, SteamDownload&) const { return false; }
  virtual bool install(std::uint32_t) { return false; }
  virtual bool remove_or_reinstall(std::uint32_t, bool) { return false; }
  virtual bool pause_download(std::uint32_t) { return false; }
  virtual bool resume_download(std::uint32_t) { return false; }
  virtual bool cancel_download(std::uint32_t) { return false; }
  virtual bool can_cancel_download() const { return false; }
  virtual bool dismiss_download(std::uint32_t) { return false; }
  virtual bool play(std::uint32_t) { return false; }
  // Confirms this app still needs registration and all writers are idle.
  virtual bool finish_setup(std::uint32_t) { return false; }
  virtual bool resolve_cloud(std::uint32_t, bool) { return false; }
  virtual bool play_without_cloud(std::uint32_t) { return false; }
  virtual bool set_remember(bool) { return false; }
  // Per-game controller layout; false when unavailable for this game.
  virtual bool controls(std::uint32_t, SteamControls &) const { return false; }
  virtual bool set_controls(std::uint32_t, SteamControls) { return false; }
};
}
