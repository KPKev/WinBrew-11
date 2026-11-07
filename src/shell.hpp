// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "icon_layout.hpp"
#include "notepad_document.hpp"
#include "paint.hpp"
#include "game_inventory.hpp"
#include "steam_ui.hpp"
#include "vapor_shortcuts.hpp"
#include "web_history.hpp"
#include "explorer_fileops.hpp"
#include <cstddef>
#include <cstdint>
namespace ww {
constexpr int W = 1920, H = 1080;
constexpr size_t AtlasBytes = icons::Bytes;
enum class Action { none, quake, playden, browser };
enum class InputDevice { controller, mouse, keyboard };
enum class TextTarget { none, search, address, document, vapor_search };
enum class Key {
  start,
  back,
  next,
  previous,
  left,
  right,
  up,
  down,
  accept,
  library,
  erase
};
struct Rect {
  int x, y, w, h;
  bool has(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};
struct FileEntry {
  char name[256]{};
  bool directory = false;
};
struct Hit {
  Rect r;
  int id;
};
struct Shell {
  Shell() = default;
  ~Shell() {
    if (owns_buffers) {
      delete[] pixels;
      delete[] wallpaper;
      delete[] font;
    }
  }
  Shell(const Shell &) = delete;
  Shell &operator=(const Shell &) = delete;
  uint32_t *pixels = nullptr;
  uint32_t *wallpaper = nullptr;
  unsigned char *font = nullptr;
  bool owns_buffers = false;
  struct GlyphBounds { std::uint8_t left = 48, top = 56, right = 0, bottom = 1; };
  GlyphBounds glyph_bounds[3 * 96]{};
  std::string desktop_shortcut_labels[VaporShortcuts::Capacity];
  std::uint64_t shortcut_labels_context = 0, shortcut_labels_next_scan_ms = 0;
  const SteamUi* shortcut_labels_source = nullptr;
  bool start = false, all = false, calendar = false, context = false,
       dialog = false, dark = false;
  int window = 0, restored = 0, focus = -1, hover = -1, mx = 1100, my = 650,
      wx = 360, wy = 155;
  bool maximized = false, drag = false;
  InputDevice input_device = InputDevice::controller,
              drag_device = InputDevice::controller;
  TextTarget text_target = TextTarget::none;
  bool touch_keyboard = false, keyboard_shift = false, keyboard_symbols = false,
       text_selection = false, keyboard_connected = false,
       mouse_connected = false;
  int text_caret = 0, mouse_speed = 5;
  int dragx = 0, dragy = 0, context_x = 0, context_y = 0;
  int context_submenu = 0;
  bool context_classic = false;
  bool pad_touch_active = false, pad_touch_pressed = false;
  bool pad_touch_test_active = false, pad_touch_test_pressed = false, pad_touch_suppress_release = false;
  bool pad_touch_drag_locked = false, pad_touch_hold_handled = false, pad_touch_moved = false;
  int pad_touch_x = 0, pad_touch_y = 0, pad_touch_id = -1;
  int pad_touch_press_x = 0, pad_touch_press_y = 0;
  std::uint64_t pad_touch_press_us = 0;
  int touchpad_deadzone = 4, touchpad_speed = 5, touchpad_acceleration = 5, touchpad_hold_ms = 600;
  float touchpad_fraction_x = 0, touchpad_fraction_y = 0;
  std::uint64_t pad_touch_motion_us = 0, pad_gesture_start_us = 0;
  int pad_gesture_mode = 0, pad_gesture_x = 0, pad_gesture_y = 0;
  bool pad_gesture_moved = false, pad_gesture_physical = false;
  struct DesktopIconPosition { std::uint64_t key = 0; int x = 0, y = 0; };
  DesktopIconPosition desktop_positions[133]{};
  int desktop_position_count = 0;
  std::uint64_t pad_touch_icon_key = 0;
  int pad_touch_icon_dx = 0, pad_touch_icon_dy = 0;
  FileEntry files[128]{};
  int file_count = 0, file_page = 0, file_error = 0, file_location = 400;
  bool file_truncated = false, file_games = false, file_preview_open = false;
  char file_path[1024] = "", file_root[1024] = "", file_preview[1024] = "";
  char file_error_stage[16] = "";
  fileops::Clipboard file_clipboard;
  fileops::Transfer file_transfer;
  fileops::Info file_properties_info;
  fileops::State file_transfer_seen = fileops::State::idle;
  bool file_context = false, file_properties = false, file_context_item = false;
  int file_selected = -1;
  Rect file_context_bounds{}, file_content_bounds{}, file_window_bounds{};
  char file_context_path[1024] = "", file_context_folder[1024] = "",
       file_properties_path[1024] = "";
  TextDocument document;
  Paint paint;
  games::Inventory game_inventory;
  int library_selection = -1, library_page = 0;
  bool library_downloads = false, steam_panel = false;
  SteamUi *steam = nullptr;
  VaporShortcuts vapor_shortcuts;
  std::uint32_t rendered_shortcut_apps[VaporShortcuts::Capacity]{};
  int steam_selection = -1, steam_scroll = 0, steam_download_offset = 0;
  bool steam_finish_setup_requested = false;
  std::uint32_t steam_setup_confirmation_app = 0;
  std::uint32_t steam_manage_confirmation_app = 0;
  bool steam_manage_reinstall = false;
  int steam_home_columns = 4;
  int steam_home_row = 0, steam_home_items[6]{-1,-1,-1,-1,-1,-1};
  int steam_recent_offset = 0, steam_recent_count = 0;
  std::uint32_t steam_rendered_recent[3]{};
  std::uint32_t steam_detail_job_app = 0;
  std::uint32_t steam_controls_app = 0;  // game whose CONTROLS button was drawn last frame
  std::uint32_t steam_selected_app = 0, steam_rendered_rows[20]{}, steam_rendered_cards[6]{};
  int steam_download_rows[5]{-1,-1,-1,-1,-1}, steam_active_download = -1, steam_queue_count = 0;
  SteamDownload steam_rendered_downloads[5]{}, steam_rendered_active{};
  std::vector<int> steam_rows, steam_library_rows, steam_home_rows;
  std::uint64_t steam_rows_signature = 0, steam_rows_context = 0, steam_rows_next_scan_ms = 0;
  std::size_t steam_favorite_count = 0;
  struct SteamRecentGame { std::uint64_t played; std::uint32_t app; int index; };
  std::vector<SteamRecentGame> steam_recent_games;
  bool vapor_favorites_only = false, vapor_favorites_collapsed = false, vapor_home_installed_only = false, vapor_home_recent_first = false;
  std::uint32_t vapor_transfer_app = 0;
  std::uint64_t vapor_transfer_rate = 0, vapor_transfer_peak = 0, vapor_transfer_tick_ms = 0;
  bool vapor_peak_known = false;
  std::uint64_t vapor_rate_history[60]{};
  bool vapor_rate_known[60]{};
  int vapor_rate_samples = 0;
  Rect steam_list_bounds{}, steam_scroll_track{}, steam_home_bounds{}, steam_home_track{}, steam_queue_bounds{}, steam_queue_track{};
  int steam_visible_rows = 8;
  bool steam_scroll_drag = false, steam_home_drag = false, steam_queue_drag = false;
  float steam_scroll_fraction = 0;
  std::uint32_t steam_art_app = 0;
  std::string steam_description, steam_genres;
  SteamNewsState steam_news_state = SteamNewsState::pending;
  std::string steam_news_title, steam_news_feed, steam_news_description;
  std::uint64_t steam_news_published = 0;
  std::vector<std::uint32_t> vapor_mark_pixels;
  int vapor_mark_width = 0, vapor_mark_height = 0;
  std::vector<std::uint32_t> vapor_badge_pixels;
  int vapor_badge_width = 0, vapor_badge_height = 0;
  int steam_art_width = 0, steam_art_height = 0;
  bool steam_art_fallback = false;
  std::vector<std::uint32_t> steam_art_pixels;
  int steam_logo_width = 0, steam_logo_height = 0;
  std::vector<std::uint32_t> steam_logo_pixels;
  struct VaporGameIcon { std::uint32_t app = 0; std::uint32_t pixels[64 * 64]{}; };
  std::vector<VaporGameIcon> vapor_game_icons;
  struct VaporGameCover { std::uint32_t app = 0; std::vector<std::uint32_t> pixels; };
  std::vector<VaporGameCover> vapor_game_covers;
  std::vector<VaporGameCover> vapor_game_portraits;
  std::vector<std::uint32_t> vapor_icon_decode;
  WebHistory web_history;
  int web_history_page = 0;
  bool notepad_prompt = false;
  int notepad_pending_action = -1, notepad_first_line = 0,
      notepad_first_column = 0;
  char notepad_pending_path[1024] = "";
  Rect notepad_body{};
  int month = 10, year = 2026, day = 28, todayMonth = 10, todayYear = 2026;
  char clock[32] = "", date[32] = "", search[80] = "";
  char vapor_search[80] = "";
  char message[256] = "";
  char address[256] = "https://www.google.com", url[768] = "";
  char preferences[1024] = "";
  bool address_edit = false, show_icons = true, left_taskbar = false,
       clock24 = false, large_pointer = false;
  bool quick = false, tray = false, ui_muted = false, audio_available = false,
       sound_test = false;
  int cursor_size = 0, pointer_speed = 5, safe_area = 0, ui_volume = 50;
  int pad_deadzone = 22;
  bool pad_right_stick = false, pad_invert_y = false, pad_swap_confirm = false,
       pad_connected = false;
  uint32_t pad_buttons = 0;
  int pad_lx = 128, pad_ly = 128, pad_rx = 128, pad_ry = 128;
  float pointer_fraction_x = 0, pointer_fraction_y = 0;
  int save_state = 0, save_error = 0, network_state = -1;
  char save_stage[32] = "", network_name[40] = "Network", network_ip[32] = "";
  char active_title[20] = "";
  int settings_page = 0, icon_size = 0, accent = 0, background = 0;
  Hit hits[128]{};
  int count = 0;
  Action pending = Action::none;
  bool owner_check_requested = false;
  int owner_check_status = -1;
  // The PS5 supplies fixed BSS-backed storage. The host may use owned storage.
  bool init(const char *assets, uint32_t *frame = nullptr,
            uint32_t *background = nullptr, unsigned char *atlas = nullptr,
            const char *settings_file = nullptr);
  void draw();
  void key(Key);
  void character(char);
  void physical_key(uint16_t usage, uint8_t modifiers = 0, uint32_t locks = 0);
  void begin_text(TextTarget);
  void end_text();
  void insert_text(const char *);
  void erase_text(bool forward = false);
  void move_caret(int);
  void clear_text();
  void commit_text();
  void draw_text_field(int, int, int, TextTarget, const char *);
  void keyboard_ui();
  void pointer(int x, int y, bool down, bool held = false,
               bool doubleclick = false);
  void scroll_steam_library(int rows);
  void scroll_steam_home(int rows);
  void scroll_steam_queue(int rows);
  void position_steam_scroll(int y);
  void position_steam_home_scroll(int y);
  void position_steam_queue_scroll(int y);
  void activate(int id);
  void result(bool ok, uint32_t code, const char *stage);
  void tick();
  void sync_steam_selection();
  void rect(Rect, uint32_t);
  void round(Rect, int, uint32_t);
  void line(int, int, int, int, uint32_t, int = 1);
  void vapor_icon(int, int, int, bool = false);
  bool vapor_game_icon(std::uint32_t, int, int, int = 22);
  void vapor_shortcut_icon(std::uint32_t, int, int, int, const char*);
  bool vapor_game_cover(std::uint32_t, Rect);
  bool vapor_game_portrait(std::uint32_t, Rect);
  void update_steam_art(std::uint32_t);
  void text(int, int, const char *, int = 0, uint32_t = 0xff202020);
  int width(const char *, int = 0);
  void text_fit(int, int, int, const char *, int, uint32_t);
  void icon(int, int, int, int);
  void button(Rect, int, const char *, int = 0);
  void add(Rect, int);
  void panel(Rect);
  void desktop_ui();
  void settings_ui(Rect);
  void browser_ui(Rect);
  void tray_ui();
  bool save_preferences();
  void load_preferences();
  void load_web_history();
  void cursor(int, int, int);
  Rect cursor_bounds(int, int) const;
  void move_stick(int, int, float);
  void controller_input(bool connected, uint32_t buttons, int lx, int ly,
                        int rx, int ry, float seconds);
  void touchpad_input(bool connected, unsigned fingers, int x, int y, int finger,
                      bool pressed, std::uint64_t timestamp_us);
  void touchpad_focus();
  void finish_touchpad_drag();
  Rect desktop_icon_position(std::uint64_t key, Rect fallback) const;
  void set_desktop_icon_position(std::uint64_t key, int x, int y);
  void load_desktop_icon_positions();
  bool save_desktop_icon_positions();
  void controller_test_ui(Rect);
  void open_context();
  void sort_desktop_by_name();
  Rect context_bounds() const;
  Rect context_submenu_bounds() const;
  void file_open(const char *);
  void file_location_open(int);
  void file_refresh();
  void file_select(int);
  void file_up();
  void explorer_ui(Rect);
  bool file_context_open();
  void file_actions_ui();
  void file_action(int);
  void file_operations_tick();
  bool notepad_open(const char *);
  bool notepad_guard(int);
  void notepad_action(int);
  void notepad_close();
  void notepad_ui(Rect);
  void notepad_prompt_ui();
  void notepad_error();
  void brand_icon(int, int, int, bool = false);
  bool navigate();
  uint32_t surface() const;
  uint32_t ink() const;
  uint32_t muted() const;
  uint32_t accent_color() const;
  void center(int, int, int, const char *, int = 0, uint32_t = 0xff202020);
};
} // namespace ww
