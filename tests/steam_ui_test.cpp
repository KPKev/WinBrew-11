// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include "../src/hid_input.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <utility>
struct FakeSteam final : ww::SteamUi {
  mutable int full_game_reads = 0, summary_reads = 0;
  std::uint32_t poll_interval = 0;
  std::uint32_t library_poll_interval_ms() const override { return poll_interval; }
  bool game_summary(std::size_t i, ww::SteamGame& out) const override {
    ++summary_reads; const bool result = game(i, out); --full_game_reads; return result;
  }
  ww::SteamStatus state{}; int scans = 0, canceled = 0, signed_out = 0, refreshed = 0;
  unsigned special_app = 0; bool special_playable = false; int played = 0, resolved = 0, skipped = 0; bool chose_cloud = false;
  bool failed = false; int retried = 0; std::string failure_text;
  ww::SteamGameActivity activity = ww::SteamGameActivity::idle;
  bool setup_allowed = false; int setup_attempts = 0;
  bool manage_allowed = false, last_reinstall = false; int manage_attempts = 0; std::uint32_t managed_app = 0;
  bool sort_fixture = false, swap_library = false; unsigned removed_app = 0; bool details_fixture = false; bool progress_fixture = false; ww::SteamInstallState progress_state = ww::SteamInstallState::running; bool installed_fixture = false; bool activity_fixture = false; bool recent_fixture = false; bool news_registration_fixture = false;
  bool queue_fixture = false, allow_cancel = true; unsigned last_resumed = 0, last_canceled_download = 0; int queue_shift = 0;
  ww::SteamDownload fake_download{}; int paused = 0; unsigned last_paused = 0;
  std::uint32_t artwork_requested = 0; std::vector<std::uint8_t> artwork_bytes, logo_bytes;
  std::uint32_t reset_app = 0;
  bool reset_game_cache(std::uint32_t app) override { reset_app = app; return true; }
  bool news_ready = false; ww::SteamArtwork news_update;
  std::vector<std::uint8_t> icon_bytes;
  std::vector<std::uint8_t> cover_bytes, portrait_bytes;
  int rejected_icons = 0, rejected_covers = 0, rejected_portraits = 0;
  void tick() override {}
  ww::SteamStatus status() const override { return state; }
  bool game(std::size_t i, ww::SteamGame& game) const override {
    ++full_game_reads;
    if (i >= state.game_count) return false;
    game.app_id = i == 0 && special_app ? special_app : unsigned(swap_library && i < 2 ? 2 - i : i + 1);
    if (game.app_id == removed_app) return false;
    game.can_play = i == 0 && special_playable;
    game.can_manage_install = i == 0 && manage_allowed;
    game.can_reinstall = game.can_manage_install;
    if (recent_fixture && i < 7 && i != 2 && i != 3) {
      game.has_last_played = true; game.last_played_unix_seconds = 1700000000 + i * 100;
    }
    if (activity_fixture && i == 1) {
      game.has_playtime = true; game.playtime_minutes = 125;
      game.has_last_played = true; game.last_played_unix_seconds = 1700000000;
    }
    if (i == 0) game.activity = activity;
    if (i == 1 && news_registration_fixture) game.activity = ww::SteamGameActivity::registration_required;
    if (details_fixture) { game.has_install_bytes = true; game.install_bytes = 2ull * 1024 * 1024 * 1024; game.has_download_bytes = true; game.download_bytes = 512ull * 1024 * 1024; }
    game.install = game.can_play ? ww::SteamInstallState::installed : ww::SteamInstallState::not_installed;
    if (installed_fixture && i == 0) game.install = ww::SteamInstallState::installed;
    if (installed_fixture && i == 2) game.install = ww::SteamInstallState::unavailable;
    if (progress_fixture && i == 1) game.install = progress_state;
    if (failed && i == 0) { game.install = ww::SteamInstallState::failed; std::snprintf(game.detail, sizeof(game.detail), "%s", failure_text.empty() ? "Storage test failure" : failure_text.c_str()); }
    if (sort_fixture && i < 2) std::snprintf(game.title, sizeof(game.title), "%s", i ? "alpha" : "Zebra");
    else std::snprintf(game.title, sizeof(game.title), "Game %02zu", i);
    return true;
  }
  void reject_game_icon(std::uint32_t) override { ++rejected_icons; }
  void reject_game_cover(std::uint32_t) override { ++rejected_covers; }
  void reject_game_portrait(std::uint32_t) override { ++rejected_portraits; }
  bool take_game_portrait(std::uint32_t app, std::vector<std::uint8_t>& bytes) override {
    if (app != 2 || portrait_bytes.empty()) return false; bytes = std::move(portrait_bytes); return true;
  }
  bool qr_module(int x, int y) const override { return (x + y) % 2 == 0; }
  bool take_game_icon(std::uint32_t app, std::vector<std::uint8_t>& bytes) override {
    if ((app != 2 && app != 3) || icon_bytes.empty()) return false; bytes = std::move(icon_bytes); return true;
  }
  bool take_game_cover(std::uint32_t app, std::vector<std::uint8_t>& bytes) override {
    if (app != 2 || cover_bytes.empty()) return false; bytes = std::move(cover_bytes); return true;
  }
  void request_artwork(std::uint32_t app) override { artwork_requested = app; }
  bool take_artwork(std::uint32_t app, ww::SteamArtwork& artwork) override {
    if (app == artwork_requested && news_ready) { artwork = news_update; news_ready = false; return true; }
    if (app != artwork_requested || artwork_bytes.empty()) return false;
    artwork.hero = std::move(artwork_bytes); artwork.logo = std::move(logo_bytes);
    artwork.description = "Test game description"; artwork.genres = "Action"; return true;
  }
  void sign_in() override { ++scans; state.account = ww::SteamAccount::approval; state.qr_size = 21; }
  void cancel() override { ++canceled; state = {}; }
  void sign_out() override { ++signed_out; state = {}; }
  void refresh() override { ++refreshed; }
  bool play(std::uint32_t) override { ++played; return true; }
  ww::SteamControls layout = ww::SteamControls::controller; int controls_set = 0;
  bool controls(std::uint32_t app, ww::SteamControls& out) const override {
    if (!special_playable || app != special_app) return false; out = layout; return true;
  }
  bool set_controls(std::uint32_t app, ww::SteamControls next) override {
    if (app != special_app) return false; layout = next; ++controls_set; return true;
  }
  bool finish_setup(std::uint32_t) override { ++setup_attempts; return setup_allowed; }
  bool download(std::size_t i, ww::SteamDownload& out) const override {
    if (i >= state.download_count || (!queue_fixture && i)) return false;
    out = fake_download;
    if (queue_fixture && i) { out.app_id = unsigned(2000 + i + queue_shift); out.state = ww::SteamInstallState::paused; }
    return true;
  }
  bool pause_download(std::uint32_t app) override { ++paused; last_paused = app; return true; }
  bool cancel_download(std::uint32_t app) override { last_canceled_download = app; return true; }
  bool can_cancel_download() const override { return allow_cancel; }
  bool resume_download(std::uint32_t app) override { ++retried; last_resumed = app; return true; }
  bool resolve_cloud(std::uint32_t app, bool cloud) override { assert(app == special_app); ++resolved; chose_cloud = cloud; return true; }
  bool play_without_cloud(std::uint32_t app) override { assert(app == special_app); ++skipped; return true; }
  bool set_remember(bool enabled) override { state.remember_enabled = enabled; return true; }
  bool remove_or_reinstall(std::uint32_t app, bool reinstall) override {
    ++manage_attempts; managed_app = app; last_reinstall = reinstall; return true;
  }
};
static ww::Rect find(ww::Shell& shell, int id) {
  shell.draw();
  for (int i = 0; i < shell.count; ++i) if (shell.hits[i].id == id) return shell.hits[i].r;
  assert(false); return {};
}
static void click(ww::Shell& shell, int id) {
  auto r = find(shell, id); shell.pointer(r.x + 5, r.y + 5, true);
}
int main(int argc, char**) {
  ww::Shell shell; assert(shell.init("assets"));
  // Steam titles are UTF-8; unsupported characters should occupy one glyph,
  // and common punctuation should use the atlas's readable equivalent.
  assert(shell.width("HELLDIVERS\xe2\x84\xa2 2") == shell.width("HELLDIVERS? 2"));
  assert(shell.width("Wild Hunt \xe2\x80\x94 Remastered") == shell.width("Wild Hunt - Remastered"));
  assert(shell.width("\xe2\x84") == shell.width("??")); // incomplete input remains safe
  assert(shell.vapor_mark_width == 744 && shell.vapor_mark_height == 171);
  assert(shell.vapor_badge_width > 0 && shell.vapor_badge_height > 0);
  shell.rect({10, 10, 36, 36}, 0xffffffffu); shell.vapor_icon(10, 10, 36, true);
  assert(shell.pixels[10 * ww::W + 10] == 0xffffffffu);
  assert(shell.pixels[28 * ww::W + 28] != 0xffffffffu);
  const auto shortcut_dir = std::filesystem::temp_directory_path() /
    ("vapor-ui-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  assert(std::filesystem::create_directory(shortcut_dir));
  const auto settings_path = (shortcut_dir / "settings.txt").string();
  assert(shell.vapor_shortcuts.load(settings_path.c_str()));
  FakeSteam steam;
  shell.steam = &steam; shell.activate(35);
  const auto downloads_button = find(shell, 966);
  assert(downloads_button.y >= shell.file_window_bounds.y + shell.file_window_bounds.h - 48);
  click(shell, 970); assert(shell.steam_panel);
  steam.state.remember_available = true;
  click(shell, 983); assert(steam.state.remember_enabled);
  click(shell, 971); assert(steam.scans == 1 && steam.state.qr_size == 21);
  shell.draw();
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-qr-ui.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  // The QR quiet zone is white and its first checker module is black.
  int left = shell.wx + 45, top = shell.wy + 250, cell = 360 / 29;
  assert(shell.pixels[(top + 1) * ww::W + left + 1] == 0xffffffff);
  assert(shell.pixels[(top + 4 * cell + 1) * ww::W + left + 4 * cell + 1] == 0xff000000);
  click(shell, 972); assert(steam.canceled == 1);
  click(shell, 971);
  steam.state.account = ww::SteamAccount::online; steam.state.library = ww::SteamLibrary::ready;
  steam.state.game_count = 32; std::snprintf(steam.state.account_name, sizeof(steam.state.account_name), "Player");
  shell.tick(); click(shell, 974);
  steam.sort_fixture = true; shell.draw();
  assert(shell.steam_rows.size() == 32 && shell.steam_rows.front() == 1 && shell.steam_rows.back() == 0);
  steam.sort_fixture = false; shell.draw();
  assert(shell.steam_recent_count == 0 && !shell.steam_rendered_recent[0]);
  steam.recent_fixture = true; shell.draw();
  assert(shell.file_window_bounds.w == 1200 && shell.steam_recent_count == 5);
  assert(shell.steam_rendered_recent[0] == 7 && shell.steam_rendered_recent[1] == 6 && shell.steam_rendered_recent[2] == 5);
  click(shell, 1152); shell.insert_text("game 01"); shell.draw();
  assert(shell.steam_recent_count == 1 && shell.steam_rendered_recent[0] == 2 && !shell.steam_rendered_recent[1]);
  click(shell, 1153); shell.end_text(); shell.draw();
  assert(shell.steam_recent_count == 5 && shell.steam_recent_offset == 0);
  const auto recent_tile = find(shell, 1040), search = find(shell, 1152);
  assert(recent_tile.x >= search.x + search.w && recent_tile.y + recent_tile.h < shell.file_window_bounds.y + 336);
  assert(recent_tile.w >= 260 && recent_tile.h == 168);
  const auto home_portrait = find(shell, 1030);
  assert(home_portrait.h == home_portrait.w * 3 / 2);
  assert(home_portrait.y >= shell.steam_home_bounds.y && home_portrait.y + home_portrait.h <= shell.steam_home_bounds.y + shell.steam_home_bounds.h);
  // Installed filter applies consistently to sidebar and Home, including unavailable storage.
  steam.installed_fixture = true; shell.draw();
  const auto filter = find(shell, 1046);
  assert(filter.x >= shell.steam_list_bounds.x && filter.x + filter.w <= shell.steam_list_bounds.x + shell.steam_list_bounds.w);
  assert(filter.y + filter.h < search.y);
  click(shell, 1046); shell.draw();
  assert(shell.vapor_home_installed_only && shell.steam_home_rows.size() == 2);
  assert(shell.steam_rendered_cards[0] == 1 && shell.steam_rendered_cards[1] == 3);
  assert(shell.steam_rows.size() == 2 && shell.steam_recent_count == 1);
  click(shell, 1152); shell.insert_text("game 00"); shell.draw();
  assert(shell.steam_home_rows.size() == 1 && shell.steam_rendered_cards[0] == 1);
  click(shell, 1153); shell.end_text(); shell.draw(); assert(shell.steam_home_rows.size() == 2);
  steam.installed_fixture = false; shell.draw(); assert(shell.steam_home_rows.empty());
  shell.focus = 1046; shell.key(ww::Key::accept); shell.draw();
  assert(!shell.vapor_home_installed_only && shell.steam_home_rows.size() == 32);
  // Sort by actual last-played data; unknown timestamps remain alphabetical.
  const auto sort_button = find(shell, 1047);
  assert(sort_button.x > shell.steam_list_bounds.x + shell.steam_list_bounds.w && sort_button.y + sort_button.h < home_portrait.y);
  click(shell, 1047); shell.draw();
  assert(shell.vapor_home_recent_first && shell.steam_rendered_cards[0] == 7 && shell.steam_rendered_cards[1] == 6);
  assert(shell.steam_home_rows[5] == 2 && shell.steam_home_rows[6] == 3); // unplayed Game 02, Game 03
  assert(shell.steam_rendered_recent[0] == 7 && shell.steam_rows.front() == 0);
  steam.installed_fixture = true; click(shell, 1046); shell.draw();
  assert(shell.steam_home_rows.size() == 2 && shell.steam_rendered_cards[0] == 1 && shell.steam_rendered_cards[1] == 3);
  click(shell, 1152); shell.insert_text("game 02"); shell.draw();
  assert(shell.steam_home_rows.size() == 1 && shell.steam_rendered_cards[0] == 3);
  click(shell, 1030); assert(shell.steam_selected_app == 3);
  shell.activate(985); click(shell, 1153); shell.end_text(); shell.draw();
  click(shell, 1046); steam.installed_fixture = false; shell.draw();
  shell.focus = 1047; shell.key(ww::Key::accept); shell.draw();
  assert(!shell.vapor_home_recent_first && shell.steam_rendered_cards[0] == 1);
  // The filter remains reachable from a selected game and never changes its identity.
  click(shell, 1031); assert(shell.steam_selected_app == 2);
  steam.installed_fixture = true; shell.draw();
  shell.focus = 1046; shell.key(ww::Key::accept); shell.draw();
  assert(shell.vapor_home_installed_only && shell.steam_selected_app == 2 && shell.steam_selection == 1);
  assert(shell.steam_rows.size() == 2 && shell.steam_rows[0] == 0 && shell.steam_rows[1] == 2);
  assert(shell.focus == 1046);
  shell.key(ww::Key::accept); shell.draw(); assert(!shell.vapor_home_installed_only && shell.steam_selected_app == 2);
  shell.activate(985); steam.installed_fixture = false; shell.draw();
  const auto first_recent = shell.steam_rendered_recent[0];
  shell.scroll_steam_home(1); shell.draw();
  assert(shell.steam_home_row == 1 && shell.steam_rendered_recent[0] == first_recent && shell.steam_rendered_cards[0] == std::uint32_t(shell.steam_home_columns + 1));
  shell.scroll_steam_home(-1); shell.draw();
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-recent-full-width.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  shell.focus = 1042; shell.key(ww::Key::right); assert(shell.steam_recent_offset == 1); shell.draw();
  assert(shell.steam_rendered_recent[0] == 6 && shell.steam_rendered_recent[2] == 2);
  shell.focus = 1040; shell.key(ww::Key::left); assert(shell.steam_recent_offset == 0);
  shell.steam_recent_offset = 2; shell.draw(); assert(shell.steam_rendered_recent[1] == 2);
  steam.swap_library = true; shell.activate(1041);
  assert(shell.steam_selected_app == 2 && shell.steam_selection == 0);
  steam.swap_library = false; shell.tick(); shell.activate(985);
  steam.recent_fixture = false; shell.steam_recent_offset = 0; shell.draw();
  assert(shell.steam_recent_count == 0 && !shell.steam_rendered_recent[0]);
  shell.focus = 1000; shell.key(ww::Key::down); assert(shell.focus == 1001);
  shell.focus = 1030; shell.key(ww::Key::right); assert(shell.focus == 1031);
  shell.key(ww::Key::down); assert(shell.steam_home_row == 1);
  shell.key(ww::Key::up); assert(shell.steam_home_row == 0);
  // Navigating from a full row to a partial final row must retain focus on a
  // real card, so confirm opens the displayed game rather than a blank slot.
  steam.state.game_count = 31; shell.draw();
  const int columns = shell.steam_home_columns;
  const int last_row = int((steam.state.game_count + columns - 1) / columns) - 1;
  shell.scroll_steam_home(last_row - 1); shell.draw();
  shell.focus = 1030 + columns - 1; shell.key(ww::Key::down); shell.draw();
  const int last_slot = int(steam.state.game_count - std::size_t(last_row * columns)) - 1;
  assert(shell.steam_home_row == last_row && shell.focus == 1030 + last_slot);
  const auto last_app = shell.steam_rendered_cards[last_slot]; assert(last_app != 0);
  shell.key(ww::Key::accept); assert(shell.steam_selected_app == last_app);
  shell.activate(985); steam.state.game_count = 32; shell.steam_home_row = 0; shell.draw();
  click(shell, 1152); shell.insert_text("gAmE 01"); shell.draw();
  assert(shell.steam_rows.size() == 1 && shell.steam_rows[0] == 1);
  click(shell, 1153); shell.draw(); assert(shell.steam_rows.size() == 32);
  shell.end_text();
  shell.focus = 1000 + shell.steam_visible_rows - 1; shell.key(ww::Key::down);
  assert(shell.steam_scroll == 1);
  shell.scroll_steam_library(-100); shell.draw();
  click(shell, 1001); assert(shell.steam_selection == 1 && shell.pending == ww::Action::none);
  assert(shell.steam_selected_app == 2);
  steam.swap_library = true; shell.tick();
  assert(shell.steam_selection == 0 && shell.steam_selected_app == 2);
  steam.removed_app = 2; shell.tick();
  assert(shell.steam_selection == -1 && shell.steam_selected_app == 0);
  steam.removed_app = 0; steam.swap_library = false; shell.draw();
  // A rendered row still opens its displayed game after the library reorders.
  steam.swap_library = true; shell.activate(1001);
  assert(shell.steam_selected_app == 2 && shell.steam_selection == 0);
  steam.swap_library = false; shell.tick(); assert(shell.steam_selection == 1);
  shell.activate(985); shell.draw();
  steam.swap_library = true; shell.activate(1030);
  assert(shell.steam_selected_app == 1 && shell.steam_selection == 1);
  steam.swap_library = false; shell.tick(); assert(shell.steam_selection == 0);
  shell.draw(); shell.activate(1001); assert(shell.steam_selected_app == 2);
  click(shell, 1160); click(shell, 1161); click(shell, 1162);
  assert(shell.vapor_shortcuts.placements(2) == 7);
  click(shell, 1151); assert(shell.vapor_shortcuts.placements(2) == 15);
  shell.draw();
  assert(shell.steam_library_rows.size() == 34 && shell.steam_library_rows[0] == 1 && shell.steam_library_rows[1] == -1);
  if (argc > 1) {
    FILE *frame = std::fopen("build/steam-favorites-review.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  assert(shell.steam_rendered_rows[0] == 2 && shell.steam_rendered_rows[1] == 0 && shell.steam_rendered_rows[2] == 1);
  // Put the unselectable divider at the viewport boundary. D-pad movement
  // must scroll to the next actual game instead of jumping out of the list.
  for (int app = 3; app <= shell.steam_visible_rows; ++app)
    assert(shell.vapor_shortcuts.toggle(unsigned(app), ww::VaporShortcuts::Favorite));
  shell.draw();
  assert(shell.steam_library_rows[std::size_t(shell.steam_visible_rows - 1)] == -1);
  shell.focus = 1000 + shell.steam_visible_rows - 2; shell.key(ww::Key::down);
  assert(shell.steam_scroll == 1 && shell.focus == 1000 + shell.steam_visible_rows - 1);
  shell.draw(); assert(shell.steam_rendered_rows[shell.steam_visible_rows - 1] != 0);
  shell.steam_scroll = shell.steam_visible_rows - 1; shell.draw();
  assert(shell.steam_library_rows[std::size_t(shell.steam_scroll)] == -1);
  shell.focus = 1001; shell.key(ww::Key::up);
  assert(shell.steam_scroll == shell.steam_visible_rows - 2 && shell.focus == 1000);
  shell.draw(); assert(shell.steam_rendered_rows[0] != 0);
  for (int app = 3; app <= shell.steam_visible_rows; ++app)
    assert(shell.vapor_shortcuts.toggle(unsigned(app), ww::VaporShortcuts::Favorite));
  shell.steam_scroll = 0; shell.draw();
  // The divider has no click target, and a rendered favorite retains identity after reorder.
  for (int i = 0; i < shell.count; ++i) assert(shell.hits[i].id != 1001);
  steam.swap_library = true; shell.activate(1000); assert(shell.steam_selected_app == 2 && shell.steam_selection == 0);
  steam.swap_library = false; shell.tick(); shell.draw();
  click(shell, 1045); shell.draw(); assert(shell.vapor_favorites_collapsed && shell.steam_library_rows.size() == 32);
  assert(shell.steam_rendered_rows[0] == 1);
  click(shell, 1045); shell.draw(); assert(!shell.vapor_favorites_collapsed && shell.steam_rendered_rows[0] == 2);
  click(shell, 1152); shell.insert_text("game 01"); shell.draw();
  assert(shell.steam_rows.size() == 1 && shell.steam_library_rows.size() == 3);
  assert(shell.steam_library_rows[0] == 1 && shell.steam_library_rows[1] == -1 && shell.steam_library_rows[2] == 1);
  click(shell, 1153); shell.end_text(); shell.draw();
  click(shell, 1150); shell.draw(); assert(shell.steam_rows.size() == 1 && shell.steam_rows[0] == 1);
  click(shell, 1150); shell.draw(); assert(shell.steam_rows.size() == 32);
  // A shortcut entry can shift between drawing and activation. Never launch
  // the game that moved into the old index; retain the other displayed ID.
  assert(shell.vapor_shortcuts.toggle(3, ww::VaporShortcuts::Desktop));
  shell.window = 0; shell.file_games = false; shell.draw();
  const auto displayed_second = find(shell, 1201);
  for (auto placement : {ww::VaporShortcuts::Desktop, ww::VaporShortcuts::Start,
                         ww::VaporShortcuts::Taskbar, ww::VaporShortcuts::Favorite})
    assert(shell.vapor_shortcuts.toggle(2, placement));
  shell.activate(1200); assert(shell.window == 0);
  const int before_mx = shell.mx, before_my = shell.my, before_focus = shell.focus;
  const bool before_dialog = shell.dialog;
  const auto before_device = shell.input_device;
  shell.mx = displayed_second.x + 5; shell.my = displayed_second.y + 5;
  shell.touchpad_input(true, 1, 100, 100, 10, false, 1000000);
  shell.touchpad_input(true, 1, 100, 100, 10, true, 1010000);
  shell.touchpad_input(true, 1, 100, 100, 10, true, 1610000);
  assert(shell.pad_touch_drag_locked && shell.pad_touch_icon_key == (0x100000000ULL | 3));
  shell.finish_touchpad_drag(); shell.touchpad_input(true, 0, 0, 0, 0, false, 1620000);
  shell.mx = before_mx; shell.my = before_my; shell.focus = before_focus; shell.input_device = before_device; shell.dialog = before_dialog;
  shell.activate(1201); assert(shell.window == 1 && shell.steam_selected_app == 3);
  assert(shell.vapor_shortcuts.toggle(3, ww::VaporShortcuts::Desktop));
  for (auto placement : {ww::VaporShortcuts::Desktop, ww::VaporShortcuts::Start,
                         ww::VaporShortcuts::Taskbar, ww::VaporShortcuts::Favorite})
    assert(shell.vapor_shortcuts.toggle(2, placement));
  shell.window = 0; shell.draw(); (void)find(shell, 1200);
  shell.start = true; shell.draw(); (void)find(shell, 1200); shell.start = false;
  shell.window = 0; shell.file_games = false; shell.activate(1200);
  assert(shell.window == 1 && shell.file_games && shell.steam_selection == 1);
  FILE* photo = std::fopen("tests/fixtures/paint-photo.jpg", "rb"); assert(photo);
  unsigned char chunk[4096]; size_t read;
  while ((read = std::fread(chunk, 1, sizeof(chunk), photo)) != 0)
    steam.artwork_bytes.insert(steam.artwork_bytes.end(), chunk, chunk + read);
  std::fclose(photo);
  // A hero that passes a network signature check can still fail to decode.
  // Recover using the independently requested, actual Steam cover.
  const auto intact_art = steam.artwork_bytes;
  steam.artwork_bytes = {0xff, 0xd8, 0xff, 1}; steam.cover_bytes = {0xff, 0xd8, 0xff, 1};
  shell.steam_art_app = 0; shell.update_steam_art(2);
  assert(shell.steam_art_width == 0 && shell.steam_art_fallback && steam.rejected_covers == 1);
  steam.cover_bytes = intact_art; shell.update_steam_art(2);
  assert(shell.steam_art_width > 0 && shell.steam_art_height > 0 && !shell.steam_art_fallback);
  steam.artwork_bytes = intact_art; shell.steam_art_app = 0;
  const auto sample_art = steam.artwork_bytes;
  steam.icon_bytes = {0xff, 0xd8, 0xff, 1};
  assert(!shell.vapor_game_icon(2, 20, 20) && steam.rejected_icons == 1);
  steam.cover_bytes = {0xff, 0xd8, 0xff, 1};
  assert(!shell.vapor_game_cover(2, {20, 20, 100, 50}) && steam.rejected_covers == 2);
  steam.portrait_bytes = {0xff, 0xd8, 0xff, 1};
  assert(!shell.vapor_game_portrait(2, {20, 20, 100, 150}) && steam.rejected_portraits == 1);
  steam.portrait_bytes = steam.artwork_bytes;
  assert(shell.vapor_game_portrait(2, {20, 20, 100, 150}));
  assert(shell.vapor_game_portrait(2, {20, 20, 100, 150})); // Cached independently of landscape cover.
  steam.icon_bytes = steam.artwork_bytes; assert(shell.vapor_game_icon(2, 20, 20));
  while (shell.vapor_game_icons.size() < 128) {
    ww::Shell::VaporGameIcon item; item.app = 10000 + unsigned(shell.vapor_game_icons.size());
    shell.vapor_game_icons.push_back(item);
  }
  steam.icon_bytes = steam.artwork_bytes;
  assert(shell.vapor_game_icon(3, 20, 20) && shell.vapor_game_icons.size() == 128);
  steam.icon_bytes = steam.artwork_bytes;
  assert(shell.vapor_game_icon(2, 20, 20)); // Evicted icons can be fetched again.
  for (auto& item : shell.vapor_game_icons) if (item.app == 2)
    for (auto& pixel : item.pixels) pixel = 0xff123456u;
  shell.vapor_shortcut_icon(2, 20, 20, 36, "Game 01");
  assert(shell.pixels[25 * ww::W + 25] == 0xff123456u);
  shell.vapor_shortcut_icon(2, 20, 20, 29, "Game 01");
  assert(shell.pixels[25 * ww::W + 25] == 0xff123456u);
  steam.cover_bytes = steam.artwork_bytes;
  assert(shell.vapor_game_cover(2, {20, 20, 100, 50}));
  assert(shell.vapor_game_cover(2, {20, 20, 100, 50})); // Decoded cover remains cached.
  FILE* logo = std::fopen("tests/fixtures/paint-alpha.png", "rb"); assert(logo);
  while ((read = std::fread(chunk, 1, sizeof(chunk), logo)) != 0)
    steam.logo_bytes.insert(steam.logo_bytes.end(), chunk, chunk + read);
  std::fclose(logo); shell.draw();
  assert(steam.artwork_requested == 2 && shell.steam_art_width > 0 && shell.steam_art_height > 0 && shell.steam_logo_width > 0);
  assert(shell.steam_description == "Test game description" && shell.steam_genres == "Action");
  // News may arrive as a separate update after hero and description.
  assert(shell.steam_news_state == ww::SteamNewsState::pending && shell.steam_news_title.empty());
  steam.news_update.news_state = ww::SteamNewsState::available;
  steam.news_update.news_title = "A real game update"; steam.news_update.news_feed = "Official";
  steam.news_update.news_published = 1700000000; steam.news_ready = true;
  shell.draw();
  assert(shell.steam_news_state == ww::SteamNewsState::available && shell.steam_news_title == "A real game update");
  assert(shell.steam_description == "Test game description" && shell.steam_news_feed == "Official");
  const int news_x = shell.file_window_bounds.x + 293 + 32 + ((shell.file_window_bounds.w - 294 - 64 - 24) / 2) + 24;
  const int news_y = shell.file_window_bounds.y + 479;  // 25px lower: CONTROLS row under the facts
  assert(shell.pixels[(news_y + 10) * ww::W + news_x + 10] == 0xff493b2bu);
  const int about_x = shell.file_window_bounds.x + 293 + 32;
  assert(shell.pixels[(news_y + 10) * ww::W + about_x + 10] == 0xff493b2bu);
  assert(about_x + ((shell.file_window_bounds.w - 294 - 64 - 24) / 2) < news_x);
  // Genres are actual Steam data and remain visible even without a description.
  shell.steam_description.clear(); shell.draw();
  assert(shell.pixels[(news_y + 10) * ww::W + about_x + 10] == 0xff493b2bu);
  shell.steam_description = "Test game description"; shell.draw();
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-detail-panels.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  std::vector<std::uint32_t> before_news_blurb;
  for (int yy = shell.file_window_bounds.y + 545; yy < shell.file_window_bounds.y + 563; ++yy)
    for (int xx = news_x + 14; xx < news_x + 194; ++xx)
      before_news_blurb.push_back(shell.pixels[yy * ww::W + xx]);
  steam.news_update.news_description = "Actual update details from Steam"; steam.news_ready = true;
  shell.draw();
  assert(shell.steam_news_description == "Actual update details from Steam");
  bool blurb_changed = false; std::size_t blurb_index = 0;
  for (int yy = shell.file_window_bounds.y + 545; yy < shell.file_window_bounds.y + 563; ++yy)
    for (int xx = news_x + 14; xx < news_x + 194; ++xx)
      blurb_changed |= before_news_blurb[blurb_index++] != shell.pixels[yy * ww::W + xx];
  assert(blurb_changed);
  // A longer real blurb wraps onto a second line in the minimum-size window.
  assert(shell.file_window_bounds.w == 1200 && shell.file_window_bounds.h == 780);
  std::vector<std::uint32_t> before_second_line;
  for (int yy = shell.file_window_bounds.y + 568; yy < shell.file_window_bounds.y + 586; ++yy)
    for (int xx = news_x + 14; xx < news_x + 194; ++xx)
      before_second_line.push_back(shell.pixels[yy * ww::W + xx]);
  steam.news_update.news_description = "A real update with details about gameplay and fixes that continues onto another line for players";
  steam.news_ready = true; shell.draw();
  bool second_line_changed = false; std::size_t second_index = 0;
  for (int yy = shell.file_window_bounds.y + 568; yy < shell.file_window_bounds.y + 586; ++yy)
    for (int xx = news_x + 14; xx < news_x + 194; ++xx)
      second_line_changed |= before_second_line[second_index++] != shell.pixels[yy * ww::W + xx];
  assert(second_line_changed);
  bool shortcut_control = false; for (int i = 0; i < shell.count; ++i) if (shell.hits[i].id == 1160) shortcut_control = true;
  assert(shortcut_control);
  // Status text has its own line and does not replace the actual news card.
  const int status_x = shell.file_window_bounds.x + 293 + 32;
  const int status_y = shell.file_window_bounds.y + 454;
  std::vector<std::uint32_t> before_status;
  for (int yy = status_y; yy < status_y + 18; ++yy)
    for (int xx = status_x; xx < status_x + 180; ++xx)
      before_status.push_back(shell.pixels[yy * ww::W + xx]);
  std::snprintf(steam.state.message, sizeof(steam.state.message), "Download paused"); shell.draw();
  bool status_changed = false; std::size_t status_index = 0;
  for (int yy = status_y; yy < status_y + 18; ++yy)
    for (int xx = status_x; xx < status_x + 180; ++xx)
      status_changed |= before_status[status_index++] != shell.pixels[yy * ww::W + xx];
  assert(status_changed && shell.pixels[(news_y + 10) * ww::W + news_x + 10] == 0xff493b2bu);
  steam.news_registration_fixture = true; steam.state.message[0] = 0; shell.draw();
  assert(shell.pixels[(news_y + 10) * ww::W + news_x + 10] == 0xff493b2bu);
  steam.news_registration_fixture = false; steam.state.cloud_choice_app = 2; shell.draw();
  bool cloud_control = false; for (int i = 0; i < shell.count; ++i) if (shell.hits[i].id == 980) cloud_control = true;
  assert(cloud_control && shell.pixels[(news_y + 10) * ww::W + news_x + 10] != 0xff493b2bu);
  steam.state.cloud_choice_app = 0; shell.draw();
  // A partial artwork update does not erase the independently delivered news.
  shell.draw(); assert(shell.steam_news_title == "A real game update");
  steam.news_update = {}; steam.news_update.news_state = ww::SteamNewsState::empty; steam.news_ready = true;
  shell.draw(); assert(shell.steam_news_state == ww::SteamNewsState::empty && shell.steam_news_title.empty() && shell.steam_news_description.empty());
  assert(shell.steam_description == "Test game description"); // About expands when no news exists.
  shell.update_steam_art(3);
  assert(shell.steam_news_state == ww::SteamNewsState::pending && shell.steam_news_title.empty() && shell.steam_news_description.empty());
  shell.update_steam_art(2);
  assert(shell.steam_news_state == ww::SteamNewsState::pending && shell.steam_news_title.empty());
  shell.draw();
  // The optional Steam download-size fact renders in its own column.
  const int detail_dx = shell.file_window_bounds.x + 293;
  const int detail_dw = shell.file_window_bounds.w - 294;
  const int detail_column = (detail_dw - 280) / 4;
  const int detail_x = detail_dx + 200 + detail_column;
  const int detail_y = shell.file_window_bounds.y + 351;
  std::vector<std::uint32_t> before_fact;
  for (int yy = detail_y; yy < detail_y + 46; ++yy)
    for (int xx = detail_x; xx < detail_x + detail_column - 8; ++xx)
      before_fact.push_back(shell.pixels[yy * ww::W + xx]);
  steam.details_fixture = true; shell.draw();
  bool fact_changed = false; std::size_t fact_index = 0;
  for (int yy = detail_y; yy < detail_y + 46; ++yy)
    for (int xx = detail_x; xx < detail_x + detail_column - 8; ++xx)
      fact_changed |= before_fact[fact_index++] != shell.pixels[yy * ww::W + xx];
  assert(fact_changed); steam.details_fixture = false; shell.draw();
  // Only the selected game's job may draw progress in the action row.
  steam.progress_fixture = true; steam.state.download_count = 1;
  steam.fake_download.app_id = 3; steam.fake_download.state = ww::SteamInstallState::running;
  steam.fake_download.done = 1024; steam.fake_download.total = 2048; shell.draw();
  const int progress_x = detail_dx + 200, progress_y = shell.file_window_bounds.y + 365;
  std::vector<std::uint32_t> before_progress;
  for (int yy = progress_y; yy < progress_y + 18; ++yy)
    for (int xx = progress_x; xx < progress_x + 180; ++xx)
      before_progress.push_back(shell.pixels[yy * ww::W + xx]);
  steam.fake_download.app_id = 2;
  steam.news_update = {}; steam.news_update.news_state = ww::SteamNewsState::available;
  steam.news_update.news_title = "Real Steam news"; steam.news_update.news_description = "Update details";
  steam.news_ready = true; shell.draw();
  assert(shell.steam_news_state == ww::SteamNewsState::available);
  // Real play history remains visible below news while progress owns the action row.
  const int history_y = shell.file_window_bounds.y + 631;
  const int history_x = detail_dx + 32;
  std::vector<std::uint32_t> before_history;
  for (int yy = history_y; yy < history_y + 18; ++yy)
    for (int xx = history_x; xx < history_x + 300; ++xx) before_history.push_back(shell.pixels[yy * ww::W + xx]);
  steam.activity_fixture = true; shell.draw();
  bool history_changed = false; std::size_t history_index = 0;
  for (int yy = history_y; yy < history_y + 18; ++yy)
    for (int xx = history_x; xx < history_x + 300; ++xx)
      history_changed |= before_history[history_index++] != shell.pixels[yy * ww::W + xx];
  assert(history_changed);
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-play-history.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  steam.state.cloud_choice_app = 2; shell.draw(); history_index = 0;
  for (int yy = history_y; yy < history_y + 18; ++yy)
    for (int xx = history_x; xx < history_x + 300; ++xx)
      assert(before_history[history_index++] == shell.pixels[yy * ww::W + xx]);
  steam.state.cloud_choice_app = 0; steam.activity_fixture = false; shell.draw();
  bool progress_changed = false; std::size_t progress_index = 0;
  for (int yy = progress_y; yy < progress_y + 18; ++yy)
    for (int xx = progress_x; xx < progress_x + 180; ++xx)
      progress_changed |= before_progress[progress_index++] != shell.pixels[yy * ww::W + xx];
  assert(progress_changed);
  // Both controller focus and pointer hover visibly highlight the active action.
  shell.focus = -1; shell.hover = -1; shell.draw();
  auto action = find(shell, 1048);
  const auto action_x = action.x + 8, action_y = action.y + 8;
  const auto normal_action = shell.pixels[action_y * ww::W + action_x];
  shell.focus = 1048; shell.draw();
  assert(shell.pixels[action_y * ww::W + action_x] != normal_action);
  shell.focus = -1; shell.pointer(action.x + 8, action.y + 8, false); shell.draw();
  assert(shell.hover == 1048 && shell.pixels[action_y * ww::W + action_x] != normal_action);
  shell.hover = -1; shell.focus = -1;
  // Details action controls the displayed job and rechecks its current state.
  steam.swap_library = true; shell.activate(1048);
  assert(steam.paused == 0); // Reordering cannot pause the new occupant of the old index.
  steam.swap_library = false; shell.tick(); shell.draw();
  click(shell, 1048); assert(steam.paused == 1 && steam.last_paused == 2);
  steam.progress_state = ww::SteamInstallState::paused; steam.fake_download.state = ww::SteamInstallState::paused; shell.draw();
  shell.activate(1048); assert(steam.paused == 1); // stale pause is not applied
  shell.focus = -1; shell.pointer(0, 0, false); shell.draw(); action = find(shell, 1049);
  const auto resume_normal = shell.pixels[(action.y + 8) * ww::W + action.x + 8];
  shell.focus = 1049; shell.draw(); assert(shell.pixels[(action.y + 8) * ww::W + action.x + 8] != resume_normal);
  shell.focus = -1; shell.pointer(action.x + 8, action.y + 8, false); shell.draw();
  assert(shell.hover == 1049 && shell.pixels[(action.y + 8) * ww::W + action.x + 8] != resume_normal);
  shell.focus = -1; shell.hover = -1;
  const int resume_before = steam.retried; click(shell, 1049);
  assert(steam.retried == resume_before + 1 && steam.last_resumed == 2);
  steam.progress_state = ww::SteamInstallState::queued; shell.draw();
  shell.activate(1049); assert(steam.retried == resume_before + 1);
  click(shell, 1048); assert(steam.paused == 2 && steam.last_paused == 2);
  steam.paused = 0; steam.retried = resume_before; steam.progress_state = ww::SteamInstallState::running;
  steam.progress_fixture = false; steam.state.download_count = 0; steam.fake_download = {}; shell.draw();
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-hero-ui.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  shell.draw(); shell.pointer(shell.steam_list_bounds.x + 20, shell.steam_list_bounds.y + 120, false);
  ww::HidInput mouse; unsigned char report[40]{}; report[8] = 1;
  int wheel = -1; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 100); assert(shell.steam_scroll == 3);
  wheel = -3; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 101); assert(shell.steam_scroll == 12);
  wheel = 3; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 102); assert(shell.steam_scroll == 3);
  // Overlays must not let either mouse or controller move the list behind them.
  shell.context = true; wheel = -1; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 103); assert(shell.steam_scroll == 3);
  for (int i=0; i<15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_scroll == 3); shell.context = false;
  shell.touch_keyboard = true;
  mouse.mouse(shell, report, 1, 104); assert(shell.steam_scroll == 3);
  for (int i=0; i<15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_scroll == 3); shell.touch_keyboard = false;
  shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  for (int i = 0; i < 15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_scroll > 3);
  const int scroll_position = shell.steam_scroll;
  const int expected_sidebar_index = shell.steam_library_rows[std::size_t(scroll_position + 1)];
  assert(expected_sidebar_index >= 0);
  click(shell, 1001); assert(shell.steam_selection == expected_sidebar_index);
  shell.draw(); const auto track = shell.steam_scroll_track;
  shell.pointer(track.x + 2, track.y, true, true); assert(shell.steam_scroll == 0);
  shell.pointer(track.x + 2, track.y + track.h - 1, false, true);
  assert(shell.steam_scroll == int(shell.steam_library_rows.size()) - shell.steam_visible_rows);
  shell.pointer(track.x + 2, track.y + track.h - 1, false, false);
  if (argc > 1) {
    shell.draw(); FILE* frame = std::fopen("build/steam-library-ui.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  click(shell, 985); assert(shell.steam_selection == -1);
  const auto normal_home_tile = find(shell, 1030);
  std::snprintf(steam.state.message, sizeof(steam.state.message), "Steam connection failed. Showing cached games.");
  const auto warning_home_tile = find(shell, 1030);
  assert(warning_home_tile.y == normal_home_tile.y + 38);
  steam.state.message[0] = 0;
  shell.draw(); shell.scroll_steam_home(2); assert(shell.steam_home_row == 2);
  shell.draw(); click(shell, 1030); assert(shell.steam_selection == 2 * shell.steam_home_columns);
  click(shell, 985); shell.scroll_steam_home(-100); assert(shell.steam_home_row == 0);
  shell.draw(); shell.pointer(shell.steam_home_bounds.x + 30, shell.steam_home_bounds.y + 30, false);
  wheel = -1; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 103); assert(shell.steam_home_row == 1);
  for (int i = 0; i < 15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_home_row > 1);
  shell.scroll_steam_home(-100); shell.draw();
  const auto home_track = shell.steam_home_track; assert(home_track.h > 0);
  shell.pointer(home_track.x + 2, home_track.y, true, true); assert(shell.steam_home_row == 0);
  shell.pointer(home_track.x + 2, home_track.y + home_track.h - 1, false, true);
  assert(shell.steam_home_row == int((steam.state.game_count + shell.steam_home_columns - 1) / shell.steam_home_columns) - 1);
  shell.pointer(home_track.x + 2, home_track.y + home_track.h - 1, false, false);
  shell.scroll_steam_home(-100); shell.draw();
  click(shell, 1032); assert(shell.steam_selection == 2);
  steam.failed = true; steam.special_app = 976730; steam.special_playable = false;
  shell.steam_selection = 0; shell.steam_selected_app = 976730;
  shell.maximized = false; shell.draw(); assert(shell.file_window_bounds.w == 1200 && shell.file_window_bounds.h == 780);
  const int heart_x = shell.file_window_bounds.x + shell.file_window_bounds.w - 66;
  const int heart_y = shell.file_window_bounds.y + 352;
  std::vector<std::uint32_t> heart_before;
  for (int yy = heart_y; yy < heart_y + 42; ++yy)
    for (int xx = heart_x; xx < heart_x + 42; ++xx) heart_before.push_back(shell.pixels[yy * ww::W + xx]);
  steam.failure_text = std::string(150, 'W'); shell.draw();
  std::size_t heart_index = 0;
  for (int yy = heart_y; yy < heart_y + 42; ++yy)
    for (int xx = heart_x; xx < heart_x + 42; ++xx) assert(heart_before[heart_index++] == shell.pixels[yy * ww::W + xx]);
  steam.failure_text.clear(); click(shell, 984); assert(steam.retried == 1);
  steam.failed = false; steam.special_app = 0;
  click(shell, 975); assert(steam.refreshed == 1 && shell.steam_selection == -1);
  steam.special_app = 1089130; steam.special_playable = true; steam.state.game_count = 1;
  shell.steam_scroll = 0; shell.steam_selection = 0; shell.steam_selected_app = 1089130; shell.tick();
  click(shell, 979); assert(steam.played == 1 && shell.pending == ww::Action::none);
  // CONTROLS: shown for a playable game under the facts row, cycles the layout.
  shell.draw();
  assert(shell.steam_controls_app == 1089130);
  click(shell, 1050); assert(steam.controls_set == 1 && steam.layout == ww::SteamControls::controller_pointer);
  click(shell, 1050); assert(steam.layout == ww::SteamControls::mouse);
  click(shell, 1050); click(shell, 1050); assert(steam.layout == ww::SteamControls::controller && steam.controls_set == 4);
  shell.draw();
  if (argc > 1) {
    FILE* frame = std::fopen("build/steam-controls.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  steam.activity = ww::SteamGameActivity::registration_required;
  click(shell, 986); assert(shell.dialog && steam.setup_attempts == 0);
  click(shell, 60); assert(!shell.dialog && !shell.steam_finish_setup_requested);
  click(shell, 986); shell.key(ww::Key::back);
  assert(!shell.dialog && !shell.steam_setup_confirmation_app && !shell.steam_finish_setup_requested);
  click(shell, 986); click(shell, 61); assert(steam.setup_attempts == 1 && !shell.steam_finish_setup_requested);
  steam.setup_allowed = true;
  click(shell, 986); click(shell, 61); assert(steam.setup_attempts == 2 && shell.steam_finish_setup_requested);
  shell.steam_finish_setup_requested = false; steam.activity = ww::SteamGameActivity::idle;
  steam.manage_allowed = true;
  click(shell, 1170); assert(shell.dialog && shell.steam_manage_confirmation_app == 1089130);
  click(shell, 60); assert(!shell.dialog && steam.manage_attempts == 0);
  click(shell, 1171); assert(shell.dialog && shell.steam_manage_reinstall);
  shell.key(ww::Key::back); assert(!shell.dialog && steam.manage_attempts == 0);
  click(shell, 1170); click(shell, 61);
  assert(steam.manage_attempts == 1 && steam.managed_app == 1089130 && !steam.last_reinstall);
  click(shell, 1171); click(shell, 61);
  assert(steam.manage_attempts == 2 && steam.managed_app == 1089130 && steam.last_reinstall);
  click(shell, 1170); steam.manage_allowed = false; click(shell, 61);
  assert(steam.manage_attempts == 2);
  steam.manage_allowed = false;
  assert(shell.vapor_shortcuts.toggle(1089130, ww::VaporShortcuts::Desktop));
  shell.window = 0; shell.file_games = false; shell.draw(); shell.activate(1201);
  assert(steam.played == 2 && shell.file_games && shell.steam_selection == 0);
  steam.state.cloud_choice_app = 1089130;
  click(shell, 980); assert(steam.resolved == 1 && !steam.chose_cloud);
  click(shell, 981); assert(steam.resolved == 2 && steam.chose_cloud);
  steam.state.cloud_choice_app = 0;
  steam.state.cloud_skip_app = 1089130; click(shell, 982); assert(steam.skipped == 1);
  steam.state.cloud_skip_app = 0;
  steam.special_playable = false; steam.state.downloads_available = true; shell.game_inventory.count = 0;
  (void)find(shell, 978);
  shell.game_inventory.count = 1; shell.game_inventory.entries[0].steam_app = 1089130;
  (void)find(shell, 30);
  shell.game_inventory.count = 0;
  steam.state.download_count = 1; steam.fake_download.app_id = 1089130;
  steam.artwork_bytes = sample_art;
  std::snprintf(steam.fake_download.title, sizeof(steam.fake_download.title), "Sample game");
  steam.fake_download.state = ww::SteamInstallState::running; steam.fake_download.has_rate = true;
  steam.fake_download.bytes_per_second = 4 * 1024 * 1024; steam.fake_download.done = 25; steam.fake_download.total = 100;
  steam.fake_download.has_eta = true; steam.fake_download.eta_seconds = INT_MAX;
  shell.tick(); assert(shell.vapor_transfer_app == 1089130 && shell.vapor_rate_samples > 0 && shell.vapor_peak_known);
  const auto observed_peak = shell.vapor_transfer_peak;
  assert(shell.vapor_rate_known[0]);
  steam.fake_download.has_rate = false; shell.vapor_transfer_tick_ms = 0; shell.tick();
  assert(shell.vapor_rate_samples == 2 && shell.vapor_rate_known[0] && !shell.vapor_rate_known[1]);
  assert(shell.vapor_transfer_peak == observed_peak);
  steam.fake_download.has_rate = true; shell.vapor_transfer_tick_ms = 0; shell.tick();
  assert(shell.vapor_rate_samples == 3 && shell.vapor_rate_known[2]);
  shell.vapor_rate_samples = 60; shell.vapor_rate_known[1] = false;
  shell.vapor_transfer_tick_ms = 0; shell.tick();
  assert(shell.vapor_rate_samples == 60 && !shell.vapor_rate_known[0] && shell.vapor_rate_known[59]);
  click(shell, 966); (void)find(shell, 1105);
  shell.steam_art_app = 1089130; shell.steam_art_width = shell.steam_art_height = 1;
  shell.steam_art_pixels.assign(1, 0xffffffffu); shell.draw();
  const auto art_x = shell.file_window_bounds.x, art_y = shell.file_window_bounds.y;
  const auto shaded = shell.pixels[(art_y + 153) * ww::W + art_x + 30];
  const auto uncovered = shell.pixels[(art_y + 153) * ww::W + art_x + shell.file_window_bounds.w / 2 - 40];
  assert(shaded != 0xffffffffu && shaded != 0xff211a17u && uncovered == 0xffffffffu);
  // Unknown speed samples remain gaps; known samples use the measured peak.
  const int graph_x = art_x + 40, graph_y = art_y + 225;
  const int graph_w = shell.file_window_bounds.w / 2 - 90;
  assert(shell.pixels[(graph_y + 10) * ww::W + graph_x] !=
         shell.pixels[(graph_y + 10) * ww::W + graph_x + 59 * graph_w / 60]);
  if (argc > 1) {
    shell.draw(); FILE* frame = std::fopen("build/vapor-download-preview.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  click(shell, 1105); assert(steam.paused == 1);
  steam.queue_fixture = true; steam.state.download_count = 9; shell.draw();
  assert(shell.steam_queue_count == 8 && shell.steam_queue_track.h > 0);
  steam.allow_cancel = false; shell.draw();
  bool pause_visible = false;
  for (int i = 0; i < shell.count; ++i) {
    assert(shell.hits[i].id != 1125 && (shell.hits[i].id < 1120 || shell.hits[i].id >= 1125));
    if (shell.hits[i].id == 1105) pause_visible = true;
  }
  assert(pause_visible);
  shell.activate(1125); shell.activate(1120);
  assert(steam.last_canceled_download == 0);
  steam.allow_cancel = true; shell.draw();
  // The first queued row changes from app 2001 to app 2002 without a redraw.
  // Cancel must follow the displayed app, never the new occupant of its index.
  steam.queue_shift = 1;
  shell.activate(1120); assert(steam.last_canceled_download == 0);
  steam.queue_shift = -1;
  shell.activate(1120); assert(steam.last_canceled_download == 2001);
  steam.queue_shift = 0;
  shell.draw();
  steam.fake_download.app_id = 9999;
  shell.activate(1125); assert(steam.last_canceled_download == 2001);
  steam.fake_download.app_id = 1089130;
  steam.fake_download.state = ww::SteamInstallState::paused;
  shell.activate(1125); assert(steam.last_canceled_download == 2001);
  steam.fake_download.state = ww::SteamInstallState::running;
  shell.draw();
  shell.pointer(shell.steam_queue_bounds.x + 30, shell.steam_queue_bounds.y + 10, false);
  wheel = -1; std::memcpy(report + 24, &wheel, sizeof(wheel));
  mouse.mouse(shell, report, 1, 104); assert(shell.steam_download_offset == 1);
  shell.steam_panel = true;
  mouse.mouse(shell, report, 1, 105);
  for (int i=0; i<15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_download_offset == 1); shell.steam_panel = false;
  shell.draw(); click(shell, 1100); assert(steam.last_resumed == 2002);
  shell.draw(); const auto queue_track = shell.steam_queue_track;
  shell.pointer(queue_track.x + 2, queue_track.y + queue_track.h - 1, true, true);
  assert(shell.steam_download_offset == 3);
  shell.pointer(queue_track.x + 2, queue_track.y, false, true); assert(shell.steam_download_offset == 0);
  shell.pointer(queue_track.x + 2, queue_track.y, false, false);
  shell.pointer(shell.steam_queue_bounds.x + 30, shell.steam_queue_bounds.y + 10, false);
  for (int i = 0; i < 15; ++i) shell.controller_input(true, 0, 128, 128, 128, 255, .05f);
  assert(shell.steam_download_offset > 0);
  steam.queue_fixture = false;
  click(shell, 965); shell.steam_selection = 2; shell.steam_selected_app = 3;
  shell.controller_input(true, 0x8000u, 128, 128, 128, 128, 0); assert(shell.library_downloads);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0);
  shell.context = true;
  shell.controller_input(true, 0x2000u, 128, 128, 128, 128, 0);
  assert(shell.library_downloads && !shell.context);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0);
  shell.controller_input(true, 0x2000u, 128, 128, 128, 128, 0);
  assert(!shell.library_downloads && shell.steam_selection == -1);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0); shell.pad_swap_confirm = true;
  shell.controller_input(true, 0x8000u, 128, 128, 128, 128, 0); assert(shell.library_downloads);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0);
  shell.controller_input(true, 0x2000u, 128, 128, 128, 128, 0); assert(!shell.library_downloads);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0); shell.pad_swap_confirm = false;
  steam.state.download_count = 0;
  click(shell, 970); click(shell, 973);
  assert(steam.signed_out == 1 && shell.steam_selection == -1 && shell.steam_scroll == 0);
  shell.window = 0; shell.file_games = false; shell.context = false;
  shell.controller_input(true, 0x2000u, 128, 128, 128, 128, 0);
  assert(shell.context); // Circle retains desktop right-click behavior.
  shell.touchpad_deadzone = 0; shell.touchpad_speed = 5; shell.touchpad_acceleration = 0;
  shell.context = false; shell.mx = 1000; shell.my = 500;
  shell.touchpad_input(true, 1, 400, 400, 1, false, 1000000);
  assert(shell.mx == 1000 && shell.my == 500);
  shell.touchpad_input(true, 1, 450, 420, 1, false, 1010000);
  assert(shell.mx == 1050 && shell.my == 520);
  shell.touchpad_input(true, 1, 1200, 800, 2, false, 1020000);
  assert(shell.mx == 1050 && shell.my == 520); // A new finger does not jump.
  shell.touchpad_input(true, 0, 0, 0, 0, false, 1030000);
  shell.count = 0; shell.add({1040, 510, 100, 50}, 40); shell.start = false;
  shell.touchpad_input(true, 1, 100, 100, 3, true, 1040000);
  assert(!shell.start);
  shell.touchpad_input(true, 1, 100, 100, 3, false, 1100000);
  assert(shell.start); // A quick touchpad click opens on release.
  shell.touchpad_input(false, 0, 0, 0, 0, false, 1050000);
  assert(!shell.pad_touch_active && !shell.pad_touch_pressed);
  shell.start = false; shell.window = 1; shell.mx = 500; shell.my = 300;
  shell.count = 0; shell.add({490, 290, 100, 40}, 90);
  shell.touchpad_input(true, 1, 200, 200, 4, false, 2000000);
  shell.touchpad_input(true, 1, 200, 200, 4, true, 2010000); assert(!shell.drag);
  shell.touchpad_input(true, 1, 200, 200, 4, true, 2610000); assert(shell.drag && shell.pad_touch_drag_locked);
  shell.touchpad_input(true, 1, 200, 200, 4, false, 2620000);
  const int old_wx = shell.wx, old_wy = shell.wy;
  shell.controller_input(true, 0, 128, 128, 128, 128, .016f);
  shell.touchpad_input(true, 1, 250, 230, 4, false, 2630000);
  assert(shell.drag && shell.wx == old_wx + 50 && shell.wy == old_wy + 30);
  shell.touchpad_input(true, 1, 250, 230, 4, true, 2640000);
  shell.touchpad_input(true, 1, 250, 230, 4, true, 3240000);
  assert(!shell.drag && !shell.pad_touch_drag_locked);
  shell.touchpad_input(true, 1, 250, 230, 4, false, 3250000);
  shell.touchpad_input(true, 0, 0, 0, 0, false, 3260000);
  shell.mx = 500; shell.my = 300; shell.count = 0; shell.add({490, 290, 100, 40}, 90);
  shell.touchpad_input(true, 1, 200, 200, 9, false, 3400000);
  shell.touchpad_input(true, 1, 200, 200, 9, true, 3410000);
  shell.touchpad_input(true, 1, 200, 200, 9, true, 4010000);
  assert(shell.pad_touch_drag_locked);
  shell.controller_input(true, 0x2000u, 128, 128, 128, 128, 0);
  assert(!shell.pad_touch_drag_locked && !shell.drag);
  shell.controller_input(true, 0, 128, 128, 128, 128, 0);
  shell.touchpad_input(true, 1, 200, 200, 9, false, 4020000);
  std::snprintf(shell.preferences, sizeof(shell.preferences), "%s", settings_path.c_str());
  shell.window = 0; shell.file_games = false; shell.start = false; shell.draw();
  const auto desktop_icon = find(shell, 10); shell.mx = desktop_icon.x + 5; shell.my = desktop_icon.y + 5;
  shell.touchpad_input(true, 1, 100, 100, 5, false, 4000000);
  shell.touchpad_input(true, 1, 100, 100, 5, true, 4010000);
  shell.touchpad_input(true, 1, 100, 100, 5, true, 4610000);
  assert(shell.pad_touch_drag_locked && shell.pad_touch_icon_key == 10);
  shell.touchpad_input(true, 1, 100, 100, 5, false, 4620000);
  shell.touchpad_input(true, 1, 220, 170, 5, false, 4630000);
  shell.touchpad_input(true, 1, 220, 170, 5, true, 4640000);
  shell.touchpad_input(true, 1, 220, 170, 5, true, 5240000);
  assert(!shell.pad_touch_drag_locked && !shell.dialog);
  ww::Shell loaded;
  std::snprintf(loaded.preferences, sizeof(loaded.preferences), "%s", settings_path.c_str());
  loaded.load_desktop_icon_positions();
  const auto restored_icon = loaded.desktop_icon_position(10, desktop_icon);
  assert(restored_icon.x == desktop_icon.x + 120 && restored_icon.y == desktop_icon.y + 70);
  shell.touchpad_input(true, 0, 0, 0, 0, false, 5300000);
  shell.context = false; shell.touchpad_input(true, 2, 200, 200, 6, false, 6000000);
  shell.touchpad_input(true, 0, 0, 0, 0, false, 6100000); assert(shell.context);
  shell.context = false; shell.touchpad_deadzone = 5; shell.mx = 800; shell.my = 500;
  shell.touchpad_input(true, 1, 200, 200, 7, false, 7000000);
  shell.touchpad_input(true, 1, 203, 202, 7, false, 7010000);
  assert(shell.mx == 800 && shell.my == 500); // Small rocking is filtered.
  shell.touchpad_input(true, 1, 215, 200, 7, false, 7020000); assert(shell.mx > 800);
  const int steady_x = shell.mx;
  shell.touchpad_input(true, 1, 230, 200, 7, true, 7030000);
  assert(shell.mx == steady_x); // Pressing cannot nudge the pointer.
  shell.touchpad_input(false, 0, 0, 0, 0, false, 7040000);
  shell.count = 0; shell.add({790, 490, 50, 50}, 40); shell.start = false; shell.mx = 800; shell.my = 500;
  shell.touchpad_input(true, 1, 200, 200, 8, false, 8000000);
  assert(shell.focus == 40 && !shell.start);
  shell.touchpad_input(true, 0, 0, 0, 0, false, 8050000); assert(!shell.start);
  shell.count = 0; shell.window = 0; shell.start = shell.context = shell.dialog = false;
  shell.add({100, 100, 80, 40}, 101); shell.add({100, 200, 80, 40}, 102);
  shell.add({190, 145, 30, 30}, 103); shell.add({220, 100, 80, 40}, 104);
  shell.focus = 101; shell.key(ww::Key::down); assert(shell.focus == 102);
  shell.key(ww::Key::up); assert(shell.focus == 101);
  shell.key(ww::Key::right); assert(shell.focus == 104);
  shell.focus = -1; shell.mx = 120; shell.my = 220; shell.key(ww::Key::up); assert(shell.focus == 102);
  if (argc > 1) {
    shell.window = 2; shell.settings_page = 9; shell.draw();
    FILE* frame = std::fopen("build/vapor-touch-settings-preview.ppm", "wb"); assert(frame);
    std::fprintf(frame, "P6\n%d %d\n255\n", ww::W, ww::H);
    for (int i = 0; i < ww::W * ww::H; ++i) std::fwrite(shell.pixels + i, 1, 3, frame);
    std::fclose(frame);
  }
  std::filesystem::remove(settings_path + ".icons");
  std::filesystem::remove(shortcut_dir / "vapor-shortcuts.txt");
  std::filesystem::remove(shortcut_dir);
  steam.state.account = ww::SteamAccount::online; steam.state.game_count = 2;
  steam.removed_app = 0; steam.swap_library = false; steam.sort_fixture = false;
  shell.file_games = true; shell.window = 1; shell.library_downloads = false; shell.steam_panel = false;
  shell.steam_selection = 0; shell.steam_selected_app = steam.special_app ? steam.special_app : 1;
  shell.steam_art_app = shell.steam_selected_app;
  shell.steam_description = "old description";
  shell.vapor_game_icons.push_back({shell.steam_selected_app});
  const auto reset_target = shell.steam_selected_app;
  const int reads_before = steam.full_game_reads;
  shell.draw();
  assert(steam.full_game_reads - reads_before == 1); // Only the selected game's full admission checks.
  shell.steam_selection = -1; shell.steam_selected_app = 0;
  const int home_reads = steam.full_game_reads;
  shell.draw(); assert(steam.full_game_reads == home_reads);
  steam.state.game_count = 3; shell.draw(); // Discovery still updates the displayed rows.
  assert(shell.steam_rows.size() == 3);
  shell.steam_selection = 0; shell.steam_selected_app = reset_target;

  shell.draw(); assert(find(shell, 1172).w > 0);
  click(shell, 1172);
  assert(steam.reset_app == reset_target && shell.steam_description.empty());
  for (const auto& icon : shell.vapor_game_icons) assert(icon.app != reset_target);
  steam.poll_interval = 60000; steam.state.game_count = 1000;
  shell.steam_rows_next_scan_ms = 0; shell.draw();
  const int summaries_before = steam.summary_reads;
  shell.draw();
  assert(steam.summary_reads - summaries_before < 100); // No full-library scan while moving pointer.
  steam.state.game_count = 1001; shell.draw();
  assert(shell.steam_rows.size() == 1001); // Newly detected games bypass the scan timer.
  std::puts("PASS: Steam account, library scrolling, selection, refresh and sign-out UI");
}
