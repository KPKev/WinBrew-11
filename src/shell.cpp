// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include "desktop_context_layout.hpp"
#include "image_codec.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <climits>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>
namespace ww {
// The displayed row can move or disappear before input is processed. Check
// the latest UI snapshot for the same game and state; the service still owns
// its live state and processes commands asynchronously by game ID.
static bool current_download(SteamUi* steam, const SteamDownload& shown, SteamDownload& current) {
  if (!steam || !shown.app_id) return false;
  const auto count = steam->status().download_count;
  for (std::size_t i = 0; i < count; ++i) {
    SteamDownload candidate;
    if (steam->download(i, candidate) && candidate.app_id == shown.app_id && candidate.state == shown.state &&
        candidate.cleanup_failure == shown.cleanup_failure && candidate.cleanup_retryable == shown.cleanup_retryable) {
      current = candidate; return true;
    }
  }
  return false;
}
static uint32_t rgb(int r, int g, int b) {
  return 0xff000000u | uint32_t(b) << 16 | uint32_t(g) << 8 | r;
}
static uint32_t mix(uint32_t a, uint32_t b, int t) {
  uint32_t o = 0xff000000;
  for (int s = 0; s < 24; s += 8)
    o |= (((a >> s & 255) * (255 - t) + (b >> s & 255) * t) / 255) << s;
  return o;
}
bool Shell::init(const char *root, uint32_t *frame, uint32_t *background,
                 unsigned char *atlas, const char *settings_file) {
  if (pixels || wallpaper || font)
    return false;
  if (frame || background || atlas) {
    if (!frame || !background || !atlas)
      return false;
    pixels = frame;
    wallpaper = background;
    font = atlas;
  } else {
    owns_buffers = true;
    pixels = new (std::nothrow) uint32_t[W * H];
    wallpaper = new (std::nothrow) uint32_t[W * H];
    font = new (std::nothrow) unsigned char[AtlasBytes];
    if (!pixels || !wallpaper || !font)
      return false;
  }
  char path[1024];
  std::snprintf(path, sizeof(path), "%s/desktop.rgba", root);
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return false;
  bool ok = std::fread(wallpaper, 4, W * H, f) == W * H;
  std::fclose(f);
  std::snprintf(path, sizeof(path), "%s/font.bin", root);
  f = std::fopen(path, "rb");
  if (!f)
    return false;
  ok = ok && std::fread(font, 1, AtlasBytes, f) == AtlasBytes;
  std::fclose(f);
  // Immutable glyph coverage is measured once, rather than scanning the empty
  // 48x55 atlas cell around every letter on every frame.
  if (ok) for (int index = 0; index < 3 * 96; ++index) {
    auto& bounds = glyph_bounds[index];
    const auto* glyph = font + index * 48 * 56;
    for (int gy = 1; gy < 56; ++gy) for (int gx = 0; gx < 48; ++gx) if (glyph[gy * 48 + gx]) {
      bounds.left = std::min(int(bounds.left), gx); bounds.right = std::max(int(bounds.right), gx + 1);
      bounds.top = std::min(int(bounds.top), gy); bounds.bottom = std::max(int(bounds.bottom), gy + 1);
    }
  }
  std::snprintf(path, sizeof(path), "%s/steam-header-logo.png", root);
  if (FILE* logo = std::fopen(path, "rb")) {
    std::vector<unsigned char> bytes(65536);
    const auto n = std::fread(bytes.data(), 1, bytes.size(), logo);
    const bool complete = std::feof(logo) != 0;
    std::fclose(logo);
    if (complete && n) {
      vapor_mark_pixels.resize(image::MaxPixels);
      if (!image::read_memory(bytes.data(), n, vapor_mark_pixels.data(), vapor_mark_width, vapor_mark_height)) {
        vapor_mark_pixels.clear(); vapor_mark_width = vapor_mark_height = 0;
      }
    }
  }
  std::snprintf(path, sizeof(path), "%s/vapor-icon-v1.png", root);
  if (FILE* badge = std::fopen(path, "rb")) {
    std::vector<unsigned char> bytes(2 * 1024 * 1024 + 1);
    const auto n = std::fread(bytes.data(), 1, bytes.size(), badge);
    const bool complete = std::feof(badge) != 0; std::fclose(badge);
    if (complete && n && n <= 2 * 1024 * 1024) {
      vapor_badge_pixels.resize(image::MaxPixels);
      if (!image::read_memory(bytes.data(), n, vapor_badge_pixels.data(), vapor_badge_width, vapor_badge_height)) {
        vapor_badge_pixels.clear(); vapor_badge_width = vapor_badge_height = 0;
      }
    }
  }
  if (settings_file) {
    std::snprintf(preferences, sizeof(preferences), "%s", settings_file);
    load_preferences();
    vapor_shortcuts.load(settings_file);
    load_desktop_icon_positions();
  }
  load_web_history();
  tick();
  return ok;
}
void Shell::load_web_history() {
  if (!preferences[0]) return;
  char path[1024];
  std::snprintf(path, sizeof(path), "%s", preferences);
  char *slash = std::strrchr(path, '/');
#ifdef _WIN32
  char *backslash = std::strrchr(path, '\\');
  if (backslash && (!slash || backslash > slash)) slash = backslash;
#endif
  if (!slash) return;
  const size_t used = size_t(slash - path + 1);
  if (used + sizeof("recent-websites.txt") > sizeof(path)) return;
  std::memcpy(path + used, "recent-websites.txt", sizeof("recent-websites.txt"));
  web_history.load(path);
}
static int find_steam_game(SteamUi *steam, std::uint32_t app) {
  if (!steam || !app) return -1;
  const auto count = steam->status().game_count;
  for (std::size_t i = 0; i < count; ++i) {
    SteamGame game;
    if (steam->game_summary(i, game) && game.app_id == app) return int(i);
  }
  return -1;
}
void Shell::sync_steam_selection() {
  if (steam_selection < 0) { steam_selected_app = 0; return; }
  if (steam_selected_app) {
    SteamGame current;
    if (steam && steam->game_summary(std::size_t(steam_selection), current) && current.app_id == steam_selected_app) return;
    steam_selection = find_steam_game(steam, steam_selected_app);
    if (steam_selection < 0) steam_selected_app = 0;
  }
}
void Shell::tick() {
  if (steam) {
    steam->tick();
    sync_steam_selection();
    const auto state = steam->status();
    SteamDownload active;
    for (std::size_t i = 0; i < state.download_count; ++i) {
      SteamDownload candidate;
      if (steam->download(i, candidate) && candidate.state == SteamInstallState::running) { active = candidate; break; }
    }
    if (active.app_id != vapor_transfer_app) {
      vapor_transfer_app = active.app_id; vapor_transfer_peak = vapor_transfer_rate = 0;
      vapor_transfer_tick_ms = 0; vapor_rate_samples = 0; vapor_peak_known = false;
    }
    if (active.app_id) {
      vapor_transfer_rate = active.has_rate ? active.bytes_per_second : 0;
      if (active.has_rate) {
        if (!vapor_peak_known || vapor_transfer_rate > vapor_transfer_peak) vapor_transfer_peak = vapor_transfer_rate;
        vapor_peak_known = true;
      }
      const auto now = std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
      if (!vapor_transfer_tick_ms || now - vapor_transfer_tick_ms >= 1000) {
        if (vapor_rate_samples == 60) {
          for (int i = 1; i < 60; ++i) {
            vapor_rate_history[i - 1] = vapor_rate_history[i];
            vapor_rate_known[i - 1] = vapor_rate_known[i];
          }
          --vapor_rate_samples;
        }
        vapor_rate_history[vapor_rate_samples] = vapor_transfer_rate;
        vapor_rate_known[vapor_rate_samples++] = active.has_rate;
        vapor_transfer_tick_ms = now;
      }
    } else vapor_transfer_rate = 0;
    if (!state.download_count) steam_download_offset = 0;
    if (state.game_count == 0) { steam_scroll = 0; steam_home_row = 0; steam_selection = -1; }
    else {
      const auto count = steam_library_rows.size();
      const auto remaining = count > std::size_t(steam_visible_rows) ? count - steam_visible_rows : 0;
      const int maximum = remaining > INT_MAX ? INT_MAX : int(remaining);
      if (steam_scroll > maximum) steam_scroll = maximum;
      if (steam_scroll < 0) steam_scroll = 0;
      if (steam_selection >= 0 && std::size_t(steam_selection) >= state.game_count)
        steam_selection = -1;
    }
  }
  file_operations_tick();
  paint.tick(*this);
  std::time_t now = std::time(nullptr);
  auto *t = std::localtime(&now);
  if (t) {
    std::strftime(clock, sizeof(clock), clock24 ? "%H:%M" : "%I:%M %p", t);
    std::strftime(date, sizeof(date), "%m/%d/%Y", t);
    day = t->tm_mday;
    todayMonth = t->tm_mon + 1;
    todayYear = t->tm_year + 1900;
    if (!calendar) {
      month = t->tm_mon + 1;
      year = t->tm_year + 1900;
    }
  }
}
void Shell::rect(Rect r, uint32_t c) {
  unsigned a = c >> 24;
  for (int y = r.y < 0 ? 0 : r.y; y < r.y + r.h && y < H; y++)
    for (int x = r.x < 0 ? 0 : r.x; x < r.x + r.w && x < W; x++)
      pixels[y * W + x] = a == 255 ? c : mix(pixels[y * W + x], c, a);
}
void Shell::round(Rect r, int rad, uint32_t c) {
  for (int y = 0; y < r.h; y++) {
    int d = y < rad ? rad - y - 1 : y >= r.h - rad ? y - (r.h - rad) : 0;
    int inset = d ? rad - int(std::sqrt(float(rad * rad - d * d))) : 0;
    rect({r.x + inset, r.y + y, r.w - inset * 2, 1}, c);
  }
}
void Shell::line(int x, int y, int x2, int y2, uint32_t c, int t) {
  int n =
      std::abs(x2 - x) > std::abs(y2 - y) ? std::abs(x2 - x) : std::abs(y2 - y);
  for (int i = 0; i <= n; i++)
    rect({x + (x2 - x) * i / (n ? n : 1), y + (y2 - y) * i / (n ? n : 1), t, t},
         c);
}
// The bundled atlas contains ASCII glyphs. Decode UTF-8 as characters before
// applying a readable fallback, so a single unsupported character does not
// turn into several question marks in a Steam game title.
static unsigned atlas_character(const char *&s) {
  const auto *p = reinterpret_cast<const unsigned char *>(s);
  unsigned first = *p++;
  if (first < 0x80) { s = reinterpret_cast<const char *>(p); return first >= 32 && first < 127 ? first : '?'; }
  unsigned code = 0, count = 0, minimum = 0;
  if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; count = 1; minimum = 0x80; }
  else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; count = 2; minimum = 0x800; }
  else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; count = 3; minimum = 0x10000; }
  else { s = reinterpret_cast<const char *>(p); return '?'; }
  const auto *end = p;
  for (unsigned i = 0; i < count; ++i) {
    if (!*end || (*end & 0xc0) != 0x80) { s = reinterpret_cast<const char *>(p); return '?'; }
    code = (code << 6) | (*end++ & 0x3f);
  }
  if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
    s = reinterpret_cast<const char *>(p); return '?';
  }
  s = reinterpret_cast<const char *>(end);
  switch (code) {
  case 0x00a0: return ' ';
  case 0x2013: case 0x2014: case 0x2212: return '-';
  case 0x2018: case 0x2019: return '\'';
  case 0x201c: case 0x201d: return '"';
  default: return '?';
  }
}
int Shell::width(const char *s, int size) {
  int w = 0;
  while (*s) {
    unsigned c = atlas_character(s);
    w += font[(size * 96 + c - 32) * 48 * 56];
  }
  return w;
}
void Shell::text(int x, int y, const char *s, int size, uint32_t c) {
  while (*s) {
    unsigned ch = atlas_character(s);
    const int index = size * 96 + ch - 32;
    const auto *g = font + index * 48 * 56;
    const auto& bounds = glyph_bounds[index];
    const int left = std::max(int(bounds.left), -x), right = std::min(int(bounds.right), W - x);
    const int top = std::max(int(bounds.top), 1 - y), bottom = std::min(int(bounds.bottom), H + 1 - y);
    for (int gy = top; gy < bottom; ++gy)
      for (int gx = left; gx < right; ++gx) {
        const unsigned alpha = g[gy * 48 + gx];
        if (alpha) {
          auto& destination = pixels[(y + gy - 1) * W + x + gx];
          destination = alpha == 255 ? c | 0xff000000u : mix(destination, c, alpha);
        }
      }
    x += g[0];
  }
}
void Shell::center(int x, int y, int w, const char *s, int size, uint32_t c) {
  text(x + (w - width(s, size)) / 2, y, s, size, c);
}
void Shell::add(Rect r, int id) {
  if (count < HitCapacity) {
    hits[count++] = {r, id};
    if (r.has(mx, my))
      hover = id;
  }
}
void Shell::button(Rect r, int id, const char *s, int size) {
  add(r, id);
  if (hover == id || focus == id)
    round(r, 5, dark ? rgb(60, 65, 76) : rgb(225, 232, 242));
  if (focus == id) {
    rect({r.x + 4, r.y + r.h - 4, r.w - 8, 2}, rgb(0, 103, 192));
  }
  center(r.x, r.y + (r.h - (size ? 30 : 24)) / 2, r.w, s, size, ink());
}
void Shell::panel(Rect r) {
  // The opaque window below completely covers this rectangle. Previously
  // every shadow layer blended the entire window interior before overwriting
  // it: a maximized window meant tens of millions of wasted pixel blends.
  const Rect covered{r.x - 1, r.y - 1, r.w + 2, r.h + 2};
  for (int i = 18; i > 0; i--) {
    const Rect shadow{r.x - i, r.y - i + 8, r.w + 2 * i, r.h + 2 * i};
    for (int row = 0; row < shadow.h; ++row) {
      const int d = row < 12 ? 11 - row : row >= shadow.h - 12 ? row - (shadow.h - 12) : 0;
      const int inset = d ? 12 - int(std::sqrt(float(144 - d * d))) : 0;
      const int left = shadow.x + inset, right = shadow.x + shadow.w - inset;
      const int y = shadow.y + row;
      if (covered.w > 0 && covered.h > 0 && y >= covered.y && y < covered.y + covered.h) {
        const int cy = y - covered.y;
        const int cd = cy < 11 ? 10 - cy : cy >= covered.h - 11 ? cy - (covered.h - 11) : 0;
        const int cut = cd ? 11 - int(std::sqrt(float(121 - cd * cd))) : 0;
        const int opaque_left = covered.x + cut, opaque_right = covered.x + covered.w - cut;
        if (opaque_right > opaque_left) {
          rect({left, y, std::max(0, std::min(right, opaque_left) - left), 1}, 0x03000000);
          const int start = std::max(left, opaque_right);
          rect({start, y, std::max(0, right - start), 1}, 0x03000000);
        } else rect({left, y, right - left, 1}, 0x03000000);
      } else rect({left, y, right - left, 1}, 0x03000000);
    }
  }
  round({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, 11,
        dark ? rgb(76, 81, 92) : rgb(210, 218, 228));
  round(r, 10, surface());
}
void Shell::draw() { desktop_ui(); }
void Shell::activate(int id) {
  sync_steam_selection();
  if(dialog && recycle_confirmation){
    if(id==60){recycle_confirmation=false;dialog=false;file_purge.cancel();}
    else if(id==61){
      recycle_confirmation=false;dialog=false;
      if(!file_purge.begin()){
        dialog=true;focus=60;std::snprintf(message,sizeof(message),"The Recycle Bin changed or could not be emptied. Nothing was deleted. Please review it and try again.");
      }else focus=807;
    }
    return;
  }
  if (dialog && steam_manage_confirmation_app) {
    if (id == 61) {
      const auto app = steam_manage_confirmation_app;
      const bool reinstall = steam_manage_reinstall;
      steam_manage_confirmation_app = 0; dialog = false;
      SteamGame current;
      const int index = find_steam_game(steam, app);
      if (steam && index >= 0 && steam->game(std::size_t(index), current) && current.app_id == app &&
          current.can_manage_install && (!reinstall || current.can_reinstall) && current.install == SteamInstallState::installed && current.activity == SteamGameActivity::idle)
        steam->remove_or_reinstall(app, reinstall);
    } else if (id == 60) { steam_manage_confirmation_app = 0; dialog = false; }
    return;
  }
  if (dialog && steam_setup_confirmation_app) {
    if (id == 61) {
      const auto app = steam_setup_confirmation_app;
      steam_setup_confirmation_app = 0; dialog = false;
      if (steam && steam->finish_setup(app)) steam_finish_setup_requested = true;
    } else if (id == 60) { steam_setup_confirmation_app = 0; dialog = false; }
    return;
  }
  if (files_busy() && id != 807 && id != 60) return;
  if (file_properties && id != 806 && id != 60) return;
  if(file_rename_prompt&&!dialog){
    if(id==2176||id==2177||id==2178){file_browser_action(id);return;}
    if(!(touch_keyboard&&id>=200&&id<=248))return;
  }
  if(id>=2100&&id<3100){if(window==1&&!file_games)file_browser_action(id);return;}
  if(notepad_modal() && !dialog && !notepad_modal_hit(id))return;
  if (!sheet_guard(id)) return;
  if(id>=2000&&id<2064){
    if(!notepad_guard(800)||paint.defer_action(*this,800))return;
    const char *path=desktop_rendered_paths[id-2000];fileops::Info info{};
    if(!path[0])return;
    if(desktop_files[id-2000].directory){window=1;start=context=false;std::snprintf(file_root,sizeof(file_root),"%s",desktop_folder_path);file_open(path);return;}
    if(fileops::inspect(path,info)<0)return;
    if(info.directory)file_open(path);else if(info.regular){if(SheetDocument::supports(path))sheet_open(path);else if(Paint::supports(path))paint.open(*this,path);else notepad_open(path);}return;
  }
  if (id >= 1530 && id <= 1532) { sheet_action(id); return; }
  if(file_context&&(file_new||file_menu_kind==3)&&id>=1420&&id<=1424){
    const bool naming=id==1420&&window==1&&!file_games;std::vector<std::string> before;for(const auto&e:files)before.push_back(e.name);
    file_context=file_new=false;file_menu_kind=0;activate(id);
    if(naming&&!dialog){for(int i=0;i<file_count;++i)if(files[i].directory&&std::find(before.begin(),before.end(),files[i].name)==before.end()){file_select_row(i);file_begin_rename();break;}}
    return;
  }
  if (context && id >= 1420 && id <= 1431) {
    context=false;
    activate(id);
    return;
  }
  if (context && id >= 1400 && id <= 1416) {
    if (id == 1400 || id == 1401 || id == 1404) {
      context_submenu = id == 1400 ? 1 : id == 1401 ? 2 : 3;
      focus = id;
    } else if (id == 1409) {
      context=false;file_context=true;shortcut_context_id=0;file_context_item=false;
      desktop_folder(file_context_folder,sizeof(file_context_folder));file_action(803);
    } else if (id == 1408) {
      context_classic = true;
      context_submenu = 0;
      if (context_y > H - 54 - desktop_context::classic_height)
        context_y = H - 54 - desktop_context::classic_height;
      focus = -1;
    } else if (id == 1405 || id == 1406) {
      context = false;
      activate(id == 1405 ? 100 : 101);
    } else if (id == 1402) {
      if (preferences[0]) {
        VaporShortcuts refreshed;
        if (refreshed.load(preferences)) vapor_shortcuts = refreshed;
        load_desktop_icon_positions();
      }
      context = false;
      focus = -1;
    } else if (id >= 1410 && id <= 1412) {
      icon_size = 1412 - id;
      context = false;
      focus = -1;
      save_preferences();
    } else if (id == 1415) {
      show_icons = !show_icons;
      context = false;
      focus = -1;
      save_preferences();
    } else if (id == 1416) {
      context = false;
      focus = -1;
      sort_desktop_by_name();
    }
    return;
  }
  if (id >= 800 && id <= 819) {
    if (id == 800) {
      if (!notepad_guard(id)) { file_context = false; return; }
      if (paint.defer_action(*this, id)) { file_context = false; return; }
    }
    if (!dialog) file_action(id);
    return;
  }
  if (paint.modal()) {
    if (id >= 740 && id <= 742) paint.activate(*this, id);
    return;
  }
  if (id >= 600 && id <= 619) {
    if (!dialog) notepad_action(id);
    return;
  }
  if (!notepad_guard(id)) return;
  if (id >= 1500 && id < 1900) { if (!dialog) sheet_action(id); return; }
  if ((id >= 1420 && id <= 1424) || (id >= 36 && id <= 38)) {
    if (!(id == 37 && window == 5) && paint.defer_action(*this,id)) return;
    if (id == 37) { paint.show(*this); start=context=false; return; }
    if (id == 36 && document.active) { window=4;start=context=false;begin_text(TextTarget::document);return; }
    if (id == 38 && sheet.storage.active) { window=6;start=context=false;end_text();return; }
    new_document(id == 36 ? 1421 : id == 38 ? 1423 : id); return;
  }
  if (id >= 700 && id <= 749) {
    paint.activate(*this, id);
    return;
  }
  const bool leaving_paint = (id >= 10 && id <= 13) ||
      (id >= 30 && id <= 41) || (id >= 100 && id <= 108) ||
      (id >= 400 && id <= 409) || (id >= 500 && id < 580) || id == 93;
  if (leaving_paint && paint.defer_action(*this, id)) return;
  bool text_control = id == 41 || id == 43 || id == 130 ||
                      (id >= 135 && id <= 139) || (id >= 200 && id <= 248) ||
                      id == 89;
  if (!text_control)
    end_text();
  if (id == 185) {
    if (window == 2 && settings_page == 4 && !dialog && !owner_check_requested) {
      owner_check_requested = true;
      owner_check_status = -2;
    }
    return;
  }
  if (id == 1172 && steam && file_games && !library_downloads && !steam_panel && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game_summary(std::size_t(steam_selection), selected) && selected.app_id == steam_selected_app &&
        steam->reset_game_cache(selected.app_id)) {
      const auto app = selected.app_id;
      vapor_game_icons.erase(std::remove_if(vapor_game_icons.begin(), vapor_game_icons.end(),
          [app](const auto& entry) { return entry.app == app; }), vapor_game_icons.end());
      for (auto* images : {&vapor_game_covers, &vapor_game_portraits})
        images->erase(std::remove_if(images->begin(), images->end(),
            [app](const auto& entry) { return entry.app == app; }), images->end());
      // Reset already queued the new detail request; keep its generation.
      steam_art_app = app; steam_art_width = steam_art_height = 0;
      steam_logo_width = steam_logo_height = 0; steam_art_fallback = false;
      steam_description.clear(); steam_genres.clear();
      steam_news_state = SteamNewsState::pending; steam_news_title.clear();
      steam_news_feed.clear(); steam_news_description.clear(); steam_news_published = 0;
    }
  } else if (id == 1152 && file_games) {
    begin_text(TextTarget::vapor_search);
  } else if (id == 1153 && file_games) {
    vapor_search[0] = 0; steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0;
    if (text_target == TextTarget::vapor_search) { text_caret = 0; text_selection = false; }
  } else if (id == 1150 && steam && file_games) {
    vapor_favorites_only = !vapor_favorites_only; steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0;
  } else if (id == 1047 && steam && file_games && !library_downloads && !steam_panel && steam_selection < 0) {
    vapor_home_recent_first = !vapor_home_recent_first; steam_home_row = 0;
  } else if (id == 1046 && steam && file_games && !library_downloads && !steam_panel) {
    vapor_home_installed_only = !vapor_home_installed_only;
    steam_scroll = 0; steam_home_row = 0; steam_recent_offset = 0;
  } else if (id == 1045 && steam && file_games && !library_downloads && !steam_panel) {
    vapor_favorites_collapsed = !vapor_favorites_collapsed; steam_scroll = 0;
  } else if (id == 1151 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && !vapor_shortcuts.toggle(selected.app_id, VaporShortcuts::Favorite)) {
      dialog = true; std::snprintf(message, sizeof(message), "Could not save this favorite.");
    }
  } else if (id >= 1160 && id <= 1162 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected)) {
      const auto placement = id == 1160 ? VaporShortcuts::Desktop : id == 1161 ? VaporShortcuts::Start : VaporShortcuts::Taskbar;
      if (!vapor_shortcuts.toggle(selected.app_id, placement)) {
        dialog = true;
        std::snprintf(message, sizeof(message), "Could not save the shortcut, or this area is full.");
      }
    }
  } else if (id >= 1200 && id < 1328) {
    const auto app = rendered_shortcut_apps[id - 1200];
    const auto placements = vapor_shortcuts.placements(app);
    if (!app || !(placements & (VaporShortcuts::Desktop | VaporShortcuts::Start | VaporShortcuts::Taskbar))) return;
    bool found = false; SteamGame game;
    if (steam) for (std::size_t i = 0; i < steam->status().game_count; ++i) {
      if (!steam->game(i, game) || game.app_id != app) continue;
      found = true;
      if (game.can_play) {
        window = 1; file_games = true; library_downloads = false; steam_panel = false; steam_selection = int(i); steam_selected_app = app;
        steam->play(app);
      }
      else {
        bool existing_quake = false;
        if (app == 1089130) for (int j = 0; j < game_inventory.count; ++j)
          if (game_inventory.entries[j].steam_app == app && game_inventory.entries[j].state != games::State::unavailable) existing_quake = true;
        if (existing_quake) pending = Action::quake;
        else { window = 1; file_games = true; library_downloads = false; steam_panel = false; steam_selection = int(i); steam_selected_app = app; }
      }
      break;
    }
    if (!found) { window = 1; file_games = true; library_downloads = false; steam_panel = true; }
    start = calendar = context = false;
  } else if (id == 985 && steam && file_games) {
    steam_selection = -1;
  } else if (id >= 1040 && id < 1043 && steam && file_games && !library_downloads && !steam_panel && steam_selection < 0) {
    const auto app = steam_rendered_recent[id - 1040];
    const int index = find_steam_game(steam, app);
    if (index >= 0) { steam_selection = index; steam_selected_app = app; steam_scroll = 0; }
  } else if (id == 1043 && steam && file_games && !library_downloads && !steam_panel && steam_selection < 0) {
    if (steam_recent_offset > 0) --steam_recent_offset;
  } else if (id == 1044 && steam && file_games && !library_downloads && !steam_panel && steam_selection < 0) {
    if (steam_recent_offset + 3 < steam_recent_count) ++steam_recent_offset;
  } else if (id >= 1030 && id < 1036 && steam && file_games && !library_downloads && !steam_panel) {
    const auto app = steam_rendered_cards[id - 1030];
    const int index = find_steam_game(steam, app);
    if (index >= 0) { steam_selection = index; steam_selected_app = app; steam_scroll = 0; }
  } else if ((id == 1048 || id == 1049) && steam && file_games && !library_downloads && !steam_panel &&
             steam_detail_job_app && steam_detail_job_app == steam_selected_app) {
    SteamGame selected;
    const int index = find_steam_game(steam, steam_detail_job_app);
    if (index >= 0 && steam->game(std::size_t(index), selected) && selected.app_id == steam_detail_job_app) {
      if (id == 1048 && (selected.install == SteamInstallState::running || selected.install == SteamInstallState::queued))
        steam->pause_download(selected.app_id);
      else if (id == 1049 && selected.install == SteamInstallState::paused)
        steam->resume_download(selected.app_id);
    }
  } else if (id == 984 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && selected.install == SteamInstallState::failed)
      steam->resume_download(selected.app_id);
  } else if (id == 982 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && selected.app_id == steam->status().cloud_skip_app)
      steam->play_without_cloud(selected.app_id);
  } else if ((id == 980 || id == 981) && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && selected.app_id == steam->status().cloud_choice_app)
      steam->resolve_cloud(selected.app_id, id == 981);
  } else if (id == 986 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) &&
        selected.activity == SteamGameActivity::registration_required && selected.app_id) {
      steam_setup_confirmation_app = selected.app_id; dialog = true; focus = 60;
    }
  } else if ((id == 1170 || id == 1171) && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && selected.app_id && selected.can_manage_install && (id != 1171 || selected.can_reinstall) &&
        selected.install == SteamInstallState::installed && selected.activity == SteamGameActivity::idle) {
      steam_manage_confirmation_app = selected.app_id; steam_manage_reinstall = id == 1171; dialog = true; focus = 60;
    }
  } else if (id == 1050 && steam && file_games && steam_selection >= 0 && steam_controls_app) {
    SteamGame selected;
    SteamControls current{};
    if (steam->game(std::size_t(steam_selection), selected) && selected.app_id == steam_controls_app &&
        steam->controls(selected.app_id, current))
      steam->set_controls(selected.app_id, steam_controls_next(current));
  } else if (id == 979 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    if (steam->game(std::size_t(steam_selection), selected) && selected.can_play && selected.activity == SteamGameActivity::idle)
      steam->play(selected.app_id);
  } else if (id == 978 && steam && file_games && steam_selection >= 0) {
    SteamGame selected;
    bool existing = false;
    if (steam->game(std::size_t(steam_selection), selected)) {
      for (int i = 0; i < game_inventory.count; ++i)
        if (selected.app_id && game_inventory.entries[i].steam_app == selected.app_id) existing = true;
      if (!existing && selected.install == SteamInstallState::not_installed) steam->install(selected.app_id);
    }
  } else if (id >= 1100 && id < 1105 && steam && file_games && library_downloads) {
    SteamDownload job;
    if (current_download(steam, steam_rendered_downloads[id - 1100], job)) {
      if (job.cleanup_failure) { if (job.cleanup_retryable && steam->can_cancel_download()) steam->cancel_download(job.app_id); }
      else if (job.cleanup_failure) { /* Reconnect storage before retrying cleanup. */ }
      else if (job.state == SteamInstallState::installed) steam->dismiss_download(job.app_id);
      else if (job.state == SteamInstallState::running || job.state == SteamInstallState::queued)
        steam->pause_download(job.app_id);
      else if (job.state == SteamInstallState::paused || job.state == SteamInstallState::failed)
        steam->resume_download(job.app_id);
    }
  } else if (id >= 1120 && id < 1125 && steam && steam->can_cancel_download() && file_games && library_downloads) {
    SteamDownload job;
    if (current_download(steam, steam_rendered_downloads[id - 1120], job))
      steam->cancel_download(job.app_id);
  } else if (id == 1105 && steam && file_games && library_downloads && steam_active_download >= 0) {
    SteamDownload job;
    if (current_download(steam, steam_rendered_active, job) && job.state == SteamInstallState::running)
      steam->pause_download(job.app_id);
  } else if (id == 1125 && steam && steam->can_cancel_download() && file_games && library_downloads && steam_active_download >= 0) {
    SteamDownload job;
    if (current_download(steam, steam_rendered_active, job) && job.state == SteamInstallState::running)
      steam->cancel_download(job.app_id);
  } else if (id == 983 && steam && file_games) {
    const auto state = steam->status();
    if (state.remember_available && !state.remember_busy) steam->set_remember(!state.remember_enabled);
  } else if (id == 970 && steam && file_games) {
    steam_panel = true;
  } else if (id == 971 && steam && file_games) {
    steam->sign_in(); steam_panel = true;
    const auto state = steam->status().account;
    if (state == SteamAccount::signing_in || state == SteamAccount::approval) {
      steam_selection = -1; steam_scroll = 0;
    }
  } else if (id == 972 && steam && file_games) {
    steam->cancel();
  } else if (id == 973 && steam && file_games) {
    steam->sign_out(); steam_panel = true;
    const auto state = steam->status().account;
    if (state == SteamAccount::signing_out || state == SteamAccount::signed_out) {
      steam_selection = -1; steam_scroll = 0;
    }
  } else if (id == 974 && steam && file_games) {
    steam_panel = false;
  } else if (id == 975 && steam && file_games) {
    steam_selection = -1; steam->refresh();
  } else if (id >= 1000 && id < 1020 && steam && file_games) {
    const auto app = steam_rendered_rows[id - 1000];
    const int index = find_steam_game(steam, app);
    if (index >= 0) { steam_selection = index; steam_selected_app = app; }
  } else if (id == 965) {
    library_downloads = false; steam_panel = false;
  } else if (id == 966) {
    library_downloads = true; steam_panel = false;
  } else if (id == 967 && library_page > 0) {
    --library_page;
  } else if (id == 968 && (library_page + 1) * 8 < game_inventory.count) {
    ++library_page;
  } else if (id >= 900 && id < 900 + games::MaximumGames) {
    if (file_games && id - 900 < game_inventory.count)
      library_selection = id - 900;
  } else if (id == 964) {
    library_selection = -1;
  } else if (id >= 850 && id <= 854) {
    const int index = web_history_page * 5 + id - 850;
    if (index < web_history.count) {
      std::snprintf(address, sizeof(address), "%s", web_history.sites[index]);
      navigate();
    }
  } else if (id == 855) {
    web_history.clear();
    web_history_page = 0;
  } else if (id == 856 && web_history_page > 0) {
    --web_history_page;
  } else if (id == 857 && (web_history_page + 1) * 5 < web_history.count) {
    ++web_history_page;
  } else if (id == 858) {
    web_history.save();
  } else if ((id >= 400 && id <= 409)||(id>=420&&id<=436)) {
    file_location_open(id);
  } else if (id >= 500 && id < 580) {
    file_select(file_hit_index(id));
  } else if (id == 35) {
    games::refresh(game_inventory);
    library_selection = -1;
    library_page = 0;
    library_downloads = false; steam_panel = false;
    window = 1;
    file_games = true;
    start = calendar = context = false;
  } else if (id == 10 || id == 32) {
    if(id==10||(!file_path[0]&&file_tabs.empty()))file_location_open(420);
    file_games=false;window = 1;
    start = calendar = context = false;
  } else if (id == 11) {
    char desktop[1024],trash[1100];desktop_folder(desktop,sizeof(desktop));
    std::snprintf(trash,sizeof(trash),"%s/.winbrew-trash",desktop);
    fileops::Info info{};
    if(fileops::inspect(trash,info)<0){dialog=true;std::snprintf(message,sizeof(message),"Desktop Recycle Bin is empty.");return;}
    window=1;file_games=false;file_open(trash);
  } else if (id == 13 || id == 30) {
    pending = Action::quake;
    start = calendar = context = false;
  } else if (id == 12 || id == 31) {
    pending = Action::playden;
    start = calendar = context = false;
  } else if (id == 33) {
    window = 2;
    start = calendar = context = false;
  } else if (id == 34) {
    window = 3;
    address_edit = false;
    start = calendar = context = false;
  } else if (id >= 100 && id <= 108) {
    window = 2;
    settings_page = id - 100;
    start = calendar = context = false;
  } else if (id >= 110 && id <= 124) {
    if (id == 110 || id == 111)
      dark = id == 111;
    if (id >= 112 && id <= 114)
      accent = id - 112;
    if (id == 115)
      clock24 = !clock24;
    if (id == 116 || id == 117)
      icon_size = id - 116;
    if (id == 118 || id == 119)
      left_taskbar = id == 119;
    if (id >= 120 && id <= 122)
      background = id - 120;
    if (id == 123)
      show_icons = !show_icons;
    if (id == 124) {
      large_pointer = !large_pointer;
      cursor_size = large_pointer ? 2 : 0;
    }
    pointer(mx, my, false);
    save_preferences();
    tick();
  } else if (id >= 140 && id <= 147) {
    if (id >= 140 && id <= 142)
      cursor_size = id - 140;
    if (id == 143 && pointer_speed > 1)
      --pointer_speed;
    if (id == 144 && pointer_speed < 10)
      ++pointer_speed;
    if (id >= 145 && id <= 147)
      safe_area = (id - 145) * 16;
    large_pointer = cursor_size == 2;
    pointer(mx, my, false);
    save_preferences();
  } else if (id >= 300 && id <= 309) {
    if (id == 300 && pointer_speed > 1)
      --pointer_speed;
    if (id == 301 && pointer_speed < 10)
      ++pointer_speed;
    if (id == 302 && pad_deadzone > 10)
      pad_deadzone -= 2;
    if (id == 303 && pad_deadzone < 40)
      pad_deadzone += 2;
    if (id == 304 || id == 305)
      pad_right_stick = id == 305;
    if (id == 306)
      pad_invert_y = !pad_invert_y;
    if (id == 307)
      pad_swap_confirm = !pad_swap_confirm;
    if (id == 308) {
      pointer_speed = 5;
      pad_deadzone = 22;
      pad_right_stick = pad_invert_y = pad_swap_confirm = false;
    }
    pointer_fraction_x = pointer_fraction_y = 0;
    if (id == 309) {
      settings_page = 8;
      focus = -1;
    } else
      save_preferences();
  } else if (id >= 320 && id <= 329) {
    if (id == 320 && touchpad_deadzone > 0) --touchpad_deadzone;
    if (id == 321 && touchpad_deadzone < 12) ++touchpad_deadzone;
    if (id == 322 && touchpad_speed > 1) --touchpad_speed;
    if (id == 323 && touchpad_speed < 10) ++touchpad_speed;
    if (id == 324 && touchpad_acceleration > 0) --touchpad_acceleration;
    if (id == 325 && touchpad_acceleration < 10) ++touchpad_acceleration;
    if (id == 326 && touchpad_hold_ms > 400) touchpad_hold_ms -= 100;
    if (id == 327 && touchpad_hold_ms < 1400) touchpad_hold_ms += 100;
    if (id == 328) { touchpad_deadzone = 4; touchpad_speed = touchpad_acceleration = 5; touchpad_hold_ms = 600; }
    if (id == 329) { window = 2; settings_page = 9; focus = -1; }
    else { touchpad_fraction_x = touchpad_fraction_y = 0; save_preferences(); }
  } else if (id == 150 || id == 151) {
    if (id == 150) {
      quick = !quick;
      tray = false;
    } else {
      tray = !tray;
      quick = false;
    }
    start = calendar = context = false;
  } else if (id == 170 || id == 171 || id == 172) {
    if (audio_available) {
      if (id == 170)
        ui_volume = ui_volume >= 10 ? ui_volume - 10 : 0;
      if (id == 171)
        ui_volume = ui_volume <= 90 ? ui_volume + 10 : 100;
      if (id == 172)
        ui_muted = !ui_muted;
      save_preferences();
    }
  } else if (id == 173) {
    sound_test = audio_available;
  } else if (id == 180 || id == 181) {
    if (id == 180 && mouse_speed > 1)
      --mouse_speed;
    if (id == 181 && mouse_speed < 10)
      ++mouse_speed;
    save_preferences();
  } else if (id == 183) {
    std::snprintf(address, sizeof(address), "http://example.com/");
    navigate();
  } else if (id == 130) {
    begin_text(TextTarget::address);
  } else if (id == 43) {
    begin_text(TextTarget::search);
  } else if (id == 131) {
    if (!navigate()) {
      dialog = true;
      std::snprintf(message, sizeof(message),
                    "Enter a website or search using http or https.");
    }
  } else if (id >= 132 && id <= 134) {
    const char *sites[] = {"https://www.google.com",
                           "https://www.wikipedia.org",
                           "https://www.youtube.com"};
    std::snprintf(address, sizeof(address), "%s", sites[id - 132]);
    navigate();
  } else if (id == 135) {
    if (text_target != TextTarget::address)
      begin_text(TextTarget::address);
    clear_text();
  } else if (id >= 136 && id <= 139) {
    if (id == 139) {
      erase_text();
    } else {
      const char *s = id == 136 ? "https://" : id == 137 ? ".com" : " ";
      insert_text(s);
    }
  } else if (id >= 200 && id <= 239) {
    const char *rows[] = {"1234567890", "qwertyuiop", "asdfghjkl.",
                          "zxcvbnm-_/"};
    const char *symbols[] = {"1234567890", "!@#$%^&*()",
                             "-_=+[]{};:", "\\|,.<>/?'\""};
    int row = (id - 200) / 10, col = (id - 200) % 10;
    const char *keys = keyboard_symbols ? symbols[row] : rows[row];
    char c = keys[col];
    if (keyboard_shift && id < 210)
      c = "!@#$%^&*()"[id - 200];
    if (keyboard_shift && c >= 'a' && c <= 'z')
      c -= 32;
    character(c);
    keyboard_shift = false;
  } else if (id >= 240 && id <= 248) {
    if (id == 240)
      keyboard_shift = !keyboard_shift;
    if (id == 241)
      keyboard_symbols = !keyboard_symbols;
    if (id == 242)
      character(' ');
    if (id == 243)
      erase_text();
    if (id == 244)
      move_caret(-1);
    if (id == 245)
      move_caret(1);
    if (id == 246)
      touch_keyboard = false;
    if (id == 247)
      clear_text();
    if (id == 248)
      commit_text();
  } else if (id == 48) {
    if (window) {
      restored = window;
      window = 0;
    } else {
      window = restored;
      restored = 0;
    }
  } else if (id == 40) {
    start = !start;
    calendar = context = false;
    search[0] = 0;
    all = false;
  } else if (id == 41) {
    start = true;
    all = true;
    search[0] = 0;
    calendar = false;
    begin_text(TextTarget::search);
  } else if (id == 42)
    all = !all;
  else if (id == 44)
    start = false;
  else if (id == 45) {
    calendar = !calendar;
    start = context = false;
  } else if (id == 46 || id == 47) {
    month += id == 46 ? -1 : 1;
    if (month == 0) {
      month = 12;
      year--;
    }
    if (month == 13) {
      month = 1;
      year++;
    }
  } else if (id == 51)
    context = false;
  else if (id == 60)
    dialog = false;
  else if (id == 91) {
    restored = window;
    window = 0;
  } else if (id == 92)
    maximized = !maximized;
  else if (id == 93) {
    window = 0;
    restored = 0;
  }
  if (id != 150 && id != 151 && id != 89)
    quick = tray = false;
  if (id >= 170 && id <= 173)
    quick = true;
  focus = id == 1046 || (id >= 100 && id <= 181) || (id >= 200 && id <= 248) ||
                  (id >= 300 && id <= 308) || (id >= 320 && id <= 328)
              ? id
              : -1;
}
void Shell::key(Key k) {
  if (context && k == Key::right && (focus == 1400 || focus == 1401 || focus == 1404)) {
    activate(focus);
    if (context_submenu == 1) focus = 1410;
    else if (context_submenu == 2) focus = 1416;
    else if (context_submenu == 3) focus = 1422;
    return;
  }
  if (context && context_submenu && (k == Key::left || k == Key::back)) {
    focus = context_submenu == 1 ? 1400 : context_submenu == 2 ? 1401 : 1404;
    context_submenu = 0;
    return;
  }
  if (sheet_prompt && k == Key::back) { sheet_action(1532); return; }
  if (k == Key::start) {
    activate(40);
    return;
  }
  if (k == Key::library) {
    activate(32);
    return;
  }
  if (k == Key::back) {
    if(file_drag_active||file_drag_pending){file_drag_active=file_drag_pending=false;file_drag_items.clear();pad_touch_drag_locked=false;return;}
    if (dialog) {
      if(recycle_confirmation){recycle_confirmation=false;file_purge.cancel();}
      dialog = false; steam_setup_confirmation_app = 0; steam_manage_confirmation_app = 0;
    }
    else if (files_busy()) {
      file_action(807);
      return;
    }
    else if (file_properties) {
      file_action(806);
      return;
    }
    else if (file_context) {
      file_context = false;
      focus = -1;
      return;
    }
    else if(file_rename_prompt){file_browser_action(2177);return;}
    else if (notepad_modal()) {
      notepad_action(notepad_replace?619:notepad_prompt?612:608);
      return;
    }
    else if (paint.modal()) {
      paint.activate(*this, 742);
      return;
    }
    else if (touch_keyboard || text_target != TextTarget::none)
      end_text();
    else if (window == 2 && settings_page == 8)
      settings_page = 7;
    else if (start)
      start = false;
    else if (calendar)
      calendar = false;
    else if (context)
      context = false;
    else if (quick || tray)
      quick = tray = false;
    else if (window == 3 && address_edit)
      address_edit = false;
    else if (window == 4) {
      notepad_close();
      return;
    }
    else if (window == 5 || window == 6) {
      activate(93);
      return;
    }
    else
      window = 0;
    focus = -1;
    return;
  }
  if (k == Key::erase) {
    erase_text();
    return;
  }
  if (k == Key::accept) {
    if (touch_keyboard && (focus == 130 || focus == 43 || focus == 600 || focus == 618 || focus < 0)) {
      focus = 200;
      return;
    }
    if (window == 3 && address_edit && (focus < 0 || focus == 130))
      activate(131);
    else if (focus >= 0)
      activate(focus);
    else
      pointer(mx, my, true, false, true);
    return;
  }
  int candidates[HitCapacity], candidate_hits[HitCapacity], n = 0;
  int window_start = -1;
  if (window && !start && !calendar && !quick && !tray && !context && !dialog &&
      !file_context && !file_properties && !notepad_modal() && !file_rename_prompt && !paint.modal() && !touch_keyboard) {
    const Rect wr = maximized ? Rect{0, 0, W, H - 54} : Rect{wx, wy, 1200, 780};
    for (int i = 0; i < count; ++i)
      if (hits[i].id == 89 && hits[i].r.x == wr.x && hits[i].r.y == wr.y && hits[i].r.w == wr.w && hits[i].r.h == wr.h) { window_start = i; break; }
  }
  for (int i = 0; i < count; i++) {
    int id = hits[i].id;
    if (dialog           ? (id != 60 && !((steam_setup_confirmation_app || steam_manage_confirmation_app || recycle_confirmation) && id == 61))
        : files_busy() ? id != 807
        : file_rename_prompt ? !(id==2176||id==2177||id==2178||(touch_keyboard&&id>=200&&id<=248))
        : file_properties ? id != 806
        : file_context ? !((id>=800&&id<=805)||id==808||id==809||id==811||id==812||id==814||id==815||id==816||id==817||id==818||(id>=2100&&id<2180)||((file_new||file_menu_kind==3)&&id>=1420&&id<=1424))
        : sheet_prompt   ? (id < 1530 || id > 1532)
        : notepad_modal() ? !notepad_modal_hit(id)
        : paint.modal()  ? (id < 740 || id > 742)
        : touch_keyboard ? (id < 200 || id > 248)
        : start          ? !((id >= 30 && id <= 44) || (id >= 1200 && id < 1328))
        : context        ? (id < 1400 || id > 1431)
        : calendar       ? (id < 45 || id > 47)
        : quick          ? (id != 105 && id != 150 && (id < 170 || id > 173))
        : tray           ? (id != 151 && id != 32 && id != 33 && id != 34)
                         : false)
      continue;
    if ((window && !start && !calendar && (id >= 10 && id <= 13)) || id == 89 ||
        id == 90 || id == 43)
      continue;
    if (start && !touch_keyboard && !Rect{660, 365, 600, 645}.has(hits[i].r.x + hits[i].r.w / 2, hits[i].r.y + hits[i].r.h / 2))
      continue;
    if (context && !context_bounds().has(hits[i].r.x + hits[i].r.w / 2, hits[i].r.y + hits[i].r.h / 2) &&
        !(context_submenu && context_submenu_bounds().has(hits[i].r.x + hits[i].r.w / 2, hits[i].r.y + hits[i].r.h / 2)))
      continue;
    if (window_start >= 0) {
      const Rect wr = maximized ? Rect{0, 0, W, H - 54} : Rect{wx, wy, 1200, 780};
      const int cx = hits[i].r.x + hits[i].r.w / 2, cy = hits[i].r.y + hits[i].r.h / 2;
      if (i <= window_start || !wr.has(cx, cy)) continue;
    }
    bool dup = false;
    for (int j = 0; j < n; j++)
      if (candidates[j] == id)
        dup = true;
    if (!dup) { candidates[n] = id; candidate_hits[n++] = i; }
  }
  if (!n)
    return;
  if (touch_keyboard && (focus < 200 || focus > 248)) {
    focus = 200;
    return;
  }
  if (focus < 0 && start) {
    for (int i = 0; i < n; i++)
      if (candidates[i] == 30) {
        focus = 30;
        return;
      }
  }
  const bool direction = k == Key::left || k == Key::right || k == Key::up || k == Key::down;
  bool focused_visible = false;
  for (int i = 0; i < n; ++i) if (candidates[i] == focus) focused_visible = true;
  if (direction && !focused_visible) {
    // Begin at the visible control nearest the cursor, or at the top-left of
    // the active window when the pointer is outside it.
    const Rect wr = maximized ? Rect{0, 0, W, H - 54} : Rect{wx, wy, 1200, 780};
    const bool use_pointer = window_start < 0 || wr.has(mx, my);
    long long best_score = LLONG_MAX; int best = -1;
    for (int i = 0; i < n; ++i) {
      const auto& r = hits[candidate_hits[i]].r;
      const long long dx = r.x + r.w / 2 - mx, dy = r.y + r.h / 2 - my;
      const long long score = use_pointer ? dx * dx + dy * dy : static_cast<long long>(r.y) * W + r.x;
      if (score < best_score) { best_score = score; best = candidates[i]; }
    }
    focus = best; return;
  }
  if (direction && window == 1 && file_games && !steam_panel && !dialog) {
    if (!library_downloads && focus >= 1000 && focus < 1020) {
      const int row = focus - 1000;
      // A section divider has no focus target. If it occupies the edge row,
      // scroll past it instead of sending directional focus out of the list.
      bool after = false, before = false;
      for (int i = row + 1; i < steam_visible_rows; ++i) after |= steam_rendered_rows[i] != 0;
      for (int i = 0; i < row; ++i) before |= steam_rendered_rows[i] != 0;
      if (k == Key::down && !after && std::size_t(steam_scroll + steam_visible_rows) < steam_library_rows.size()) {
        scroll_steam_library(1); focus = 1000 + steam_visible_rows - 1; return;
      }
      if (k == Key::up && !before && steam_scroll > 0) {
        scroll_steam_library(-1); focus = 1000; return;
      }
    }
    if (!library_downloads && steam_selection < 0 && focus >= 1040 && focus < 1043) {
      if (k == Key::right && focus == 1042 && steam_recent_offset + 3 < steam_recent_count) { ++steam_recent_offset; return; }
      if (k == Key::left && focus == 1040 && steam_recent_offset > 0) { --steam_recent_offset; return; }
    }
    if (!library_downloads && steam_selection < 0 && focus >= 1030 && focus < 1036) {
      const int max_row = std::max(0, int((steam_home_rows.size() + steam_home_columns - 1) / steam_home_columns) - 1);
      if (k == Key::down && steam_home_row < max_row) {
        scroll_steam_home(1);
        const auto remaining = steam_home_rows.size() - std::size_t(steam_home_row * steam_home_columns);
        const int last_card = int(std::min<std::size_t>(remaining, std::size_t(steam_home_columns))) - 1;
        if (focus - 1030 > last_card) focus = 1030 + last_card;
        return;
      }
      if (k == Key::up && steam_home_row > 0) { scroll_steam_home(-1); return; }
    }
    if (library_downloads && ((focus >= 1100 && focus < 1105) || (focus >= 1120 && focus < 1125))) {
      const int row = focus >= 1120 ? focus - 1120 : focus - 1100;
      if (k == Key::down && row == 4 && steam_download_offset < steam_queue_count - 5) { scroll_steam_queue(1); return; }
      if (k == Key::up && row == 0 && steam_download_offset > 0) { scroll_steam_queue(-1); return; }
    }
  }
  if (focus >= 0 &&
      (k == Key::left || k == Key::right || k == Key::up || k == Key::down)) {
    const Hit *from = nullptr;
    for (int i = 0; i < n; ++i)
      if (candidates[i] == focus) from = &hits[candidate_hits[i]];
    int best = -1;
    long long score = LLONG_MAX; bool best_aligned = false;
    if (from)
      for (int i = 0; i < n; ++i) {
        const auto &h = hits[candidate_hits[i]];
        if (h.id == focus)
          continue;
        int dx = h.r.x + h.r.w / 2 - from->r.x - from->r.w / 2;
        int dy = h.r.y + h.r.h / 2 - from->r.y - from->r.h / 2;
        int along = k == Key::left    ? -dx
                    : k == Key::right ? dx
                    : k == Key::up    ? -dy
                                      : dy;
        int across = (k == Key::left || k == Key::right) ? dy : dx;
        if (along <= 0)
          continue;
        const bool horizontal = k == Key::left || k == Key::right;
        const int from_low = horizontal ? from->r.y : from->r.x;
        const int from_high = from_low + (horizontal ? from->r.h : from->r.w);
        const int to_low = horizontal ? h.r.y : h.r.x;
        const int to_high = to_low + (horizontal ? h.r.h : h.r.w);
        const bool aligned = from_low < to_high && to_low < from_high;
        const long long rank = static_cast<long long>(along) * along + static_cast<long long>(across) * across;
        if ((aligned && !best_aligned) || (aligned == best_aligned && rank < score)) {
          score = rank;
          best = h.id;
          best_aligned = aligned;
        }
      }
    if (best >= 0)
      focus = best;
    return;
  }
  int at = -1;
  for (int i = 0; i < n; i++)
    if (candidates[i] == focus)
      at = i;
  int delta = (k == Key::previous || k == Key::left || k == Key::up) ? -1 : 1;
  at = (at + delta + n) % n;
  focus = candidates[at];
}
void Shell::sort_desktop_by_name() {
  struct Item { std::uint64_t key; std::string name; };
  std::vector<Item> items = {{10, "This PC"}, {11, "Recycle Bin"}, {35, "Vapor"},
                             {13, "Quake II RTX"}, {34, "Web"}};
  for (std::size_t index = 0; index < vapor_shortcuts.count(); ++index) {
    const auto &entry = vapor_shortcuts.entries()[index];
    if (!(entry.placements & VaporShortcuts::Desktop)) continue;
    std::string name = "Steam game " + std::to_string(entry.app);
    if (steam) for (std::size_t game_index = 0; game_index < steam->status().game_count; ++game_index) {
      SteamGame game;
      if (steam->game(game_index, game) && game.app_id == entry.app) { name = game.title; break; }
    }
    items.push_back({0x100000000ULL | entry.app, name});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item &left, const Item &right) {
    return std::lexicographical_compare(left.name.begin(), left.name.end(), right.name.begin(), right.name.end(),
      [](unsigned char first, unsigned char second) { return std::tolower(first) < std::tolower(second); });
  });
  const int spacing = icon_size == 2 ? 134 : icon_size == 1 ? 112 : 94;
  const int rows = (H - 170 - 18) / spacing + 1;
  for (std::size_t index = 0; index < items.size(); ++index)
    set_desktop_icon_position(items[index].key, 12 + int(index / rows) * 112,
                              18 + int(index % rows) * spacing);
  if (preferences[0] && !save_desktop_icon_positions()) {
    dialog = true;
    std::snprintf(message, sizeof(message), "Could not save desktop icon positions.");
  }
}
Rect Shell::context_bounds() const {
  return {context_x, context_y,
          context_classic ? desktop_context::classic_width : desktop_context::modern_width,
          context_classic ? desktop_context::classic_height : desktop_context::modern_height};
}
Rect Shell::context_submenu_bounds() const {
  using namespace desktop_context;
  const int width = context_submenu == 1 ? (context_classic ? classic_view_width : modern_view_width) :
                    context_submenu == 2 ? (context_classic ? classic_sort_width : modern_sort_width) :
                                           (context_classic ? classic_new_width : modern_new_width);
  const int height = context_submenu == 1 ? (context_classic ? classic_view_height : modern_view_height) :
                     context_submenu == 2 ? (context_classic ? classic_sort_height : modern_sort_height) :
                                            (context_classic ? classic_new_height : modern_new_height);
  const int top = context_submenu == 1 ? 4 : context_submenu == 2 ?
                  (context_classic ? classic_offsets[1] : modern_offsets[1]) :
                  (context_classic ? classic_offsets[11] : modern_offsets[4]);
  const int root_width = context_bounds().w;
  const int x = context_x + root_width - submenu_overlap + width <= W ?
                context_x + root_width - submenu_overlap : context_x - width + submenu_overlap;
  int y = context_y + top;
  if (y + height > H - 54)
    y = context_y + top + (context_classic ? classic_row_height : modern_row_height) - height;
  return {x < 0 ? 0 : x, y < 0 ? 0 : y, width, height};
}
void Shell::open_context() {
  if (dialog || files_busy() || file_rename_prompt || file_properties) return;
  if (file_context_open()) return;
  if (notepad_modal() || paint.modal()) return;
  context_x = mx < 0 ? 0 : mx > W - desktop_context::modern_width ? W - desktop_context::modern_width : mx;
  context_y = my < 0 ? 0 : my > H - 54 - desktop_context::modern_height ? H - 54 - desktop_context::modern_height : my;
  context_classic = false;
  context_submenu = 0;
  context = true;
  focus = -1;
  start = calendar = quick = tray = false;
  end_text();
}
void Shell::scroll_steam_library(int rows) {
  if (!steam || window != 1 || !file_games || library_downloads || steam_panel || dialog ||
      file_context || file_properties || files_busy() || touch_keyboard || context ||
      notepad_modal() || paint.modal() || start || calendar || quick || tray) return;
  const auto count = steam_library_rows.size();
  const auto remaining = count > std::size_t(steam_visible_rows) ? count - steam_visible_rows : 0;
  const int maximum = remaining > INT_MAX ? INT_MAX : int(remaining);
  long long next = static_cast<long long>(steam_scroll) + rows;
  steam_scroll = next < 0 ? 0 : next > maximum ? maximum : int(next);
}
void Shell::scroll_steam_home(int rows) {
  if (!steam || window != 1 || !file_games || library_downloads || steam_panel || dialog ||
      steam_selection >= 0 || file_context || file_properties || files_busy() || touch_keyboard || context ||
      notepad_modal() || paint.modal() || start || calendar || quick || tray) return;
  const int count = int((steam_home_rows.size() + steam_home_columns - 1) / steam_home_columns);
  const int maximum = count > 1 ? count - 1 : 0;
  const long long next = static_cast<long long>(steam_home_row) + rows;
  steam_home_row = next < 0 ? 0 : next > maximum ? maximum : int(next);
}
void Shell::scroll_steam_queue(int rows) {
  if (!steam || window != 1 || !file_games || !library_downloads || steam_panel || dialog ||
      file_context || file_properties || files_busy() || touch_keyboard || context ||
      notepad_modal() || paint.modal() || start || calendar || quick || tray) return;
  const int maximum = steam_queue_count > 5 ? steam_queue_count - 5 : 0;
  const long long next = static_cast<long long>(steam_download_offset) + rows;
  steam_download_offset = next < 0 ? 0 : next > maximum ? maximum : int(next);
}
void Shell::position_steam_scroll(int y) {
  if (!steam || steam_scroll_track.h <= 0) return;
  const auto count = steam_library_rows.size();
  if (count <= std::size_t(steam_visible_rows)) { steam_scroll = 0; return; }
  const int maximum = count - steam_visible_rows > INT_MAX ? INT_MAX : int(count - steam_visible_rows);
  int thumb = int((static_cast<long long>(steam_scroll_track.h) * steam_visible_rows) / count);
  if (thumb < 30) thumb = 30;
  if (thumb > steam_scroll_track.h) thumb = steam_scroll_track.h;
  const int travel = steam_scroll_track.h - thumb;
  if (!travel) return;
  int position = y - steam_scroll_track.y - thumb / 2;
  if (position < 0) position = 0;
  if (position > travel) position = travel;
  steam_scroll = int((static_cast<long long>(position) * maximum + travel / 2) / travel);
}
void Shell::position_steam_home_scroll(int y) {
  if (!steam || steam_home_track.h <= 0) return;
  const int count = int((steam_home_rows.size() + steam_home_columns - 1) / steam_home_columns);
  if (count <= 1) { steam_home_row = 0; return; }
  int thumb = std::max(24, steam_home_track.h / count);
  if (thumb > steam_home_track.h) thumb = steam_home_track.h;
  const int travel = steam_home_track.h - thumb;
  if (travel <= 0) return;
  int position = y - steam_home_track.y - thumb / 2;
  if (position < 0) position = 0;
  if (position > travel) position = travel;
  steam_home_row = int((static_cast<long long>(position) * (count - 1) + travel / 2) / travel);
}
void Shell::position_steam_queue_scroll(int y) {
  if (!steam || steam_queue_track.h <= 0 || steam_queue_count <= 5) return;
  int thumb = std::max(24, steam_queue_track.h * 5 / steam_queue_count);
  if (thumb > steam_queue_track.h) thumb = steam_queue_track.h;
  const int travel = steam_queue_track.h - thumb;
  if (travel <= 0) return;
  int position = y - steam_queue_track.y - thumb / 2;
  if (position < 0) position = 0;
  if (position > travel) position = travel;
  steam_download_offset = int((static_cast<long long>(position) * (steam_queue_count - 5) + travel / 2) / travel);
}
void Shell::pointer(int x, int y, bool down, bool held, bool doubleclick) {
  auto bounds = cursor_bounds(0, 0);
  int right = W - bounds.w - safe_area, bottom = H - bounds.h - safe_area;
  mx = x < safe_area ? safe_area : x > right ? right : x;
  my = y < safe_area ? safe_area : y > bottom ? bottom : y;
  if (context && !down && context_bounds().has(mx, my) &&
      !(context_submenu && context_submenu_bounds().has(mx, my))) {
    const int relative = my - context_y;
    const int *offsets = context_classic ? desktop_context::classic_offsets : desktop_context::modern_offsets;
    const int row_height = context_classic ? desktop_context::classic_row_height : desktop_context::modern_row_height;
    const int new_top = offsets[context_classic ? 11 : 4];
    int submenu = relative >= offsets[0] && relative < offsets[0] + row_height ? 1 :
                  relative >= offsets[1] && relative < offsets[1] + row_height ? 2 :
                  relative >= new_top && relative < new_top + row_height ? 3 : 0;
    if (submenu) context_submenu = submenu;
    else context_submenu = 0;
  }
  if(file_drag_active){if(held)return;file_drop();return;}
  if(file_drag_pending){
    if(held&&(std::abs(mx-file_drag_x)>6||std::abs(my-file_drag_y)>6)){file_drag_begin();if(file_drag_active)return;}
    if(!held){file_drag_pending=false;if(file_drag_collapse&&file_selected>=0)file_select_row(file_selected);file_drag_collapse=false;}
  }
  if(file_scroll_drag||file_nav_drag){if(held&&window==1&&!file_games&&!dialog&&!files_busy()){file_position_scroll(my,file_nav_drag);return;}file_scroll_drag=file_nav_drag=false;}
  if(down&&window==1&&!file_games&&!dialog&&!file_context&&!file_properties&&!file_rename_prompt&&!files_busy()){
    if(file_scroll_track.w&&file_scroll_track.has(mx,my)&&file_matches.size()>size_t(std::max(1,file_visible_count))){file_scroll_drag=true;file_position_scroll(my);return;}
    if(file_nav_track.w&&file_nav_track.has(mx,my)&&file_nav_total>file_nav_bounds.h/30){file_nav_drag=true;file_position_scroll(my,true);return;}
  }
  if (steam_queue_drag) {
    if (held && window == 1 && file_games && library_downloads && !dialog) {
      position_steam_queue_scroll(my); return;
    }
    steam_queue_drag = false;
  }
  if (down && steam && window == 1 && file_games && library_downloads && !dialog &&
      !file_context && !file_properties && !files_busy() && steam_queue_track.has(mx, my)) {
    steam_queue_drag = true; position_steam_queue_scroll(my); return;
  }
  if (steam_home_drag) {
    if (held && window == 1 && file_games && !library_downloads && !steam_panel && steam_selection < 0 && !dialog) {
      position_steam_home_scroll(my); return;
    }
    steam_home_drag = false;
  }
  if (down && steam && window == 1 && file_games && !library_downloads && !steam_panel && steam_selection < 0 && !dialog &&
      !file_context && !file_properties && !files_busy() && steam_home_track.has(mx, my)) {
    steam_home_drag = true; position_steam_home_scroll(my); return;
  }
  if (steam_scroll_drag) {
    if (held && window == 1 && file_games && !library_downloads && !steam_panel) {
      position_steam_scroll(my); return;
    }
    steam_scroll_drag = false;
  }
  if (down && steam && window == 1 && file_games && !library_downloads && !steam_panel && !dialog &&
      !file_context && !file_properties && !files_busy() && steam_scroll_track.has(mx, my)) {
    steam_scroll_drag = true; position_steam_scroll(my); return;
  }
  if (paint.pointer(*this, mx, my, down, held)) return;
  if (drag && held) {
    wx = mx - dragx;
    wy = my - dragy;
    if (wx < -1000)
      wx = -1000;
    if (wx > W - 100)
      wx = W - 100;
    if (wy < 0)
      wy = 0;
    if (wy > H - 120)
      wy = H - 120;
    return;
  }
  if (!held)
    drag = false;
  if (!down)
    return;
  if (file_context && !file_context_bounds.has(mx, my) && !(file_new&&file_new_bounds.has(mx,my))) {
    file_context = false;
    return;
  }
  if (context && !context_bounds().has(mx, my) &&
      !(context_submenu && context_submenu_bounds().has(mx, my))) {
    context = false;
    return;
  }
  if (quick && !Rect{1496, 586, 408, 430}.has(mx, my) && my < H - 54) {
    quick = false;
    return;
  }
  if (tray && !Rect{1488, 696, 416, 320}.has(mx, my) && my < H - 54) {
    tray = false;
    return;
  }
  if (start && !Rect{660, 365, 600, 645}.has(mx, my) &&
      !(touch_keyboard && Rect{460, 626, 1000, 388}.has(mx, my)) &&
      my < H - 54) {
    start = false;
    return;
  }
  if (calendar && !Rect{1490, 510, 408, 500}.has(mx, my) && my < H - 54) {
    calendar = false;
    return;
  }
  focus = -1;
  for (int i = count - 1; i >= 0; i--)
    if (hits[i].r.has(mx, my)) {
      int id = hits[i].id;
      if (context && id == 89) return;
      if (context && id != 89 && (id < 1400 || id > 1431)) return;
      if (dialog && id != 60 && !((steam_setup_confirmation_app || steam_manage_confirmation_app || recycle_confirmation) && id == 61))
        return;
      if (!dialog && files_busy() && id != 807) return;
      if (!dialog && file_rename_prompt && !(id==2176||id==2177||id==2178||(touch_keyboard&&id>=200&&id<=248)))return;
      if (!dialog && file_properties && id != 806) return;
      if (!dialog && file_context && !((id>=2100&&id<2180)||(id>=800&&id<=819)||((file_new||file_menu_kind==3)&&id>=1420&&id<=1424))) return;
      if (sheet_prompt && (id < 1530 || id > 1532)) return;
      if (notepad_modal() && !dialog && !notepad_modal_hit(id))
        return;
      if (paint.modal() && (id < 740 || id > 742)) return;
      if (id == 90 && !maximized) {
        drag = true;
        drag_device = input_device;
        dragx = mx - wx;
        dragy = my - wy;
        return;
      }
      if (id >= 500 && id < 580 && (!doubleclick||file_select_mode||(file_modifiers&0x11))) {
        focus = id;
        int index=file_hit_index(id);
        if((file_modifiers&0x22)&&file_selection_anchor>=0&&index>=0){if(!(file_modifiers&0x11))file_selection.clear();for(int candidate:file_matches)if(candidate>=std::min(file_selection_anchor,index)&&candidate<=std::max(file_selection_anchor,index)&&std::find(file_selection.begin(),file_selection.end(),files[candidate].name)==file_selection.end())file_selection.push_back(files[candidate].name);file_selected=index;}
        else if(index>=0&&file_selection.size()>1&&!(file_modifiers&0x11)&&!file_select_mode&&std::find(file_selection.begin(),file_selection.end(),files[index].name)!=file_selection.end()){file_selected=index;file_drag_collapse=true;}
        else file_select_row(index,file_select_mode||(file_modifiers&0x11));
        file_drag_pending=index>=0&&!file_selection.empty();file_drag_x=mx;file_drag_y=my;
        return;
      }
      if (((id >= 10 && id <= 13) || (id >= 1200 && id < 1328 && hits[i].r.x < 130 && hits[i].r.y < H - 54) ||
           ((id == 34 || id == 35 || (id >= 36 && id <= 38)) && hits[i].r.x < 130) || (id>=2000&&id<2064)) &&
          !doubleclick) {
        focus = id;
        return;
      }
      activate(id);
      return;
    }
  if (!context) start = calendar = false;
}
void Shell::result(bool ok, uint32_t code, const char *stage) {
  dialog = true;
  focus = 60;
  std::snprintf(message, sizeof(message),
                ok ? "Launch request accepted (%s)."
                   : "Launch unavailable: %s (0x%08X).",
                stage, code);
}
} // namespace ww
