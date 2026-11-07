// SPDX-License-Identifier: GPL-3.0-or-later
#include "explorer_directory.hpp"
#include "privilege.hpp"
#include "image_codec.hpp"
#include "vapor_game_details.hpp"
#include "shell.hpp"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <fcntl.h>
#endif
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_EXPLORER_TEST)
extern "C" {
int sceKernelOpen(const char *, int, int);
int sceKernelGetdents(int, char *, int);
int sceKernelClose(int);
}
#ifdef __PROSPERO__
static_assert(offsetof(dirent, d_reclen) == 4 &&
              offsetof(dirent, d_type) == 6 &&
              offsetof(dirent, d_namlen) == 7 && offsetof(dirent, d_name) == 8);
#endif
#endif
namespace ww {
static bool steam_date(std::uint64_t seconds, char* out, std::size_t size) {
  if (!seconds || seconds >= 4102444800ULL) return false;
  unsigned days = static_cast<unsigned>(seconds / 86400), year = 1970;
  const auto leap = [](unsigned y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
  while (days >= 365u + unsigned(leap(year))) days -= 365u + unsigned(leap(year++));
  static constexpr unsigned month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  static constexpr const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  unsigned month = 0;
  while (month < 11) {
    const unsigned length = month_days[month] + unsigned(month == 1 && leap(year));
    if (days < length) break;
    days -= length; ++month;
  }
  std::snprintf(out, size, "%s %u, %u", months[month], days + 1, year);
  return true;
}
namespace {
void file_failure(Shell &shell, int error, const char *stage) {
  shell.file_error = directory::error_or_io(error);
  std::snprintf(shell.file_error_stage, sizeof(shell.file_error_stage), "%s",
                stage);
}
bool append_file(Shell &shell, const char *name, bool folder) {
  if (!std::strcmp(name, ".") || !std::strcmp(name, ".."))
    return true;
  if (std::strlen(name) >= sizeof(shell.files[0].name))
    return true;
  if (shell.file_count == int(sizeof(shell.files) / sizeof(shell.files[0]))) {
    shell.file_truncated = true;
    return false;
  }
  auto &entry = shell.files[shell.file_count++];
  std::snprintf(entry.name, sizeof(entry.name), "%s", name);
  entry.directory = folder;
  return true;
}
const char *location_name(int id) {
  switch (id) {
  case 401:
    return "Games";
  case 402:
    return "WinBrew 11 files";
  case 403:
    return "USB drive";
  case 404:
    return "App files";
  case 409:
    return "PS5 data";
  default:
    return "This PC";
  }
}
bool usb_path(const char *path) { return !std::strncmp(path, "/mnt/usb", 8); }
} // namespace
void Shell::file_location_open(int id) {
  window = 1;
  context = start = false;
  if (id == 400 || id == 401) {
    file_location = id;
    file_games = id == 401;
    if (file_games) {
      games::refresh(game_inventory);
      library_selection = -1;
      library_page = 0;
      library_downloads = false; steam_panel = false;
    }
    file_path[0] = file_root[0] = file_preview[0] = file_error_stage[0] = 0;
    file_count = file_page = file_error = 0;
    file_preview_open = file_truncated = false;
    return;
  }
  if (id == 405) {
    file_up();
    return;
  }
  if (id == 406) {
    file_refresh();
    return;
  }
  if (id == 407) {
    if (file_page > 0)
      --file_page;
    return;
  }
  if (id == 408) {
    if ((file_page + 1) * 10 < file_count)
      ++file_page;
    return;
  }
  if (id != 402 && id != 403 && id != 404 && id != 409)
    return;
  file_location = id;
  char path[1024];
#ifdef _WIN32
  std::snprintf(path, sizeof(path), ".");
#else
  const char *target = id == 403 ? "/mnt/usb0" : id == 404 ? "/app0" : "/data";
  std::snprintf(path, sizeof(path), "%s", target);
  if (id == 402) {
    std::snprintf(path, sizeof(path), "%s", preferences);
    char *slash = std::strrchr(path, '/');
    if (slash) {
      if (slash == path)
        slash[1] = 0;
      else
        *slash = 0;
    } else {
#ifdef __PROSPERO__
      std::snprintf(path, sizeof(path), "/download0");
#else
      std::snprintf(path, sizeof(path), ".");
#endif
    }
  }
#endif
  std::snprintf(file_root, sizeof(file_root), "%s", path);
  file_open(path);
}
void Shell::file_open(const char *path) {
  if (!path || !*path || std::strlen(path) >= sizeof(file_path)) {
    file_failure(*this, path && *path ? ENAMETOOLONG : ENOENT, "Open folder");
    return;
  }
  char copy[1024];
  std::snprintf(copy, sizeof(copy), "%s", path);
  std::snprintf(file_path, sizeof(file_path), "%s", copy);
  file_page = 0;
  file_games = false;
  file_preview_open = false;
  file_preview[0] = 0;
  file_refresh();
}
void Shell::file_refresh() {
  file_context = false;
  file_selected = -1;
  file_count = 0;
  file_error = 0;
  file_error_stage[0] = 0;
  file_truncated = false;
  file_preview[0] = 0;
  file_preview_open = false;
  if (!file_path[0])
    return;
#ifdef _WIN32
  char pattern[1050];
  std::snprintf(pattern, sizeof(pattern), "%s/*", file_path);
  _finddata_t item{};
  intptr_t h = _findfirst(pattern, &item);
  if (h == -1) {
    // _findfirst reports ENOENT for a valid empty directory, too.
    struct _stat info{};
    if (_stat(file_path, &info) == 0 && (info.st_mode & _S_IFDIR))
      return;
    file_failure(*this, errno, "Open folder");
    return;
  }
  do {
    if (!append_file(*this, item.name, (item.attrib & _A_SUBDIR) != 0))
      break;
  } while (_findnext(h, &item) == 0);
  _findclose(h);
#elif defined(__PROSPERO__) || defined(WINE11_NATIVE_EXPLORER_TEST)
  // Explicit platform imports avoid libc directory-state/errno assumptions.
  // Enumeration is read-only and remains subject to the title's permissions.
  const int fd = ww::privilege::run(
      [&] { return sceKernelOpen(file_path, O_RDONLY | O_DIRECTORY, 0); });
  if (fd < 0) {
    file_failure(*this, directory::native_error(fd), "Open folder");
    return;
  }
  alignas(16) static unsigned char buffer[64 * 1024];
  bool reading = true;
  while (reading) {
    const int bytes =
        sceKernelGetdents(fd, reinterpret_cast<char *>(buffer), sizeof(buffer));
    if (bytes < 0) {
      file_failure(*this, directory::native_error(bytes), "Read folder");
      break;
    }
    if (!bytes)
      break;
    if (bytes > int(sizeof(buffer))) {
      file_failure(*this, EIO, "Read folder");
      break;
    }
    size_t offset = 0;
    directory::Record item{};
    while (true) {
      const auto state = directory::next(buffer, size_t(bytes), offset, item);
      if (state == directory::Read::end)
        break;
      if (state == directory::Read::invalid) {
        file_failure(*this, EIO, "Read folder");
        reading = false;
        break;
      }
      if (!item.inode)
        continue;
      bool folder = item.type == DT_DIR;
      if (item.type == DT_UNKNOWN || item.type == DT_LNK) {
        char path[1300];
        std::snprintf(path, sizeof(path), "%s/%s", file_path, item.name);
        const int child = ww::privilege::run(
            [&] { return sceKernelOpen(path, O_RDONLY | O_DIRECTORY, 0); });
        if (child >= 0) {
          folder = true;
          sceKernelClose(child);
        }
      }
      if (!append_file(*this, item.name, folder)) {
        reading = false;
        break;
      }
    }
  }
  const int closed = sceKernelClose(fd);
  if (closed < 0 && !file_error)
    file_failure(*this, directory::native_error(closed), "Close folder");
#else
  errno = 0;
  DIR *dir = nullptr;
  ww::privilege::run([&] { dir = opendir(file_path); return dir ? 0 : -1; });
  if (!dir) {
    file_failure(*this, errno, "Open folder");
    return;
  }
  while (true) {
    errno = 0;
    auto *item = readdir(dir);
    if (!item) {
      if (errno)
        file_failure(*this, errno, "Read folder");
      break;
    }
    const char *name = item->d_name;
    bool folder = item->d_type == DT_DIR;
    if (item->d_type == DT_UNKNOWN || item->d_type == DT_LNK) {
      char path[1300];
      std::snprintf(path, sizeof(path), "%s/%s", file_path, name);
      struct stat st{};
      folder = ww::privilege::run([&] { return stat(path, &st); }) == 0 && S_ISDIR(st.st_mode);
    }
    if (!append_file(*this, name, folder))
      break;
  }
  if (closedir(dir) != 0 && !file_error)
    file_failure(*this, errno, "Close folder");
#endif
  if (file_error) {
    file_count = 0;
    file_truncated = false;
  }
  std::sort(files, files + file_count,
            [](const FileEntry &a, const FileEntry &b) {
              if (a.directory != b.directory)
                return a.directory > b.directory;
              return std::strcmp(a.name, b.name) < 0;
            });
  if (file_page * 10 >= file_count)
    file_page = file_count ? (file_count - 1) / 10 : 0;
}
void Shell::file_select(int index) {
  if (index < 0 || index >= file_count)
    return;
  char path[1200];
  int n =
      std::snprintf(path, sizeof(path), "%s/%s", file_path, files[index].name);
  if (n < 0 || n >= int(sizeof(file_path))) {
    dialog = true;
    std::snprintf(message, sizeof(message), "Path is too long to open.");
    return;
  }
  if (files[index].directory) {
    file_open(path);
    return;
  }
  if (Paint::supports(path))
    paint.open(*this, path);
  else
    notepad_open(path);
}
void Shell::file_up() {
  if (!file_path[0])
    return;
  if (file_root[0] && !std::strcmp(file_path, file_root)) {
    file_location_open(400);
    return;
  }
  char *slash = std::strrchr(file_path, '/');
  if (!slash || slash == file_path) {
    file_location_open(400);
    return;
  }
  *slash = 0;
  file_page = 0;
  file_refresh();
}
void Shell::update_steam_art(std::uint32_t app) {
  if (!steam || !app) return;
  if (steam_art_app != app) {
    steam_art_app = app; steam_art_width = steam_art_height = 0; steam_art_fallback = false;
    steam_logo_width = steam_logo_height = 0;
    steam_description.clear(); steam_genres.clear();
    steam_news_state = SteamNewsState::pending; steam_news_title.clear(); steam_news_feed.clear(); steam_news_description.clear(); steam_news_published = 0;
    steam->request_artwork(app);
  }
  SteamArtwork artwork;
  (void)steam->take_artwork(app, artwork);
  if (!artwork.description.empty()) steam_description = std::move(artwork.description);
  if (!artwork.genres.empty()) steam_genres = std::move(artwork.genres);
  if (artwork.news_state != SteamNewsState::pending) {
    steam_news_state = artwork.news_state;
    steam_news_title = std::move(artwork.news_title);
    steam_news_feed = std::move(artwork.news_feed);
    steam_news_description = std::move(artwork.news_description);
    steam_news_published = artwork.news_published;
  }
  if (!artwork.hero.empty()) {
    if (steam_art_pixels.empty()) steam_art_pixels.resize(image::MaxPixels);
    if (!image::read_memory(artwork.hero.data(), artwork.hero.size(), steam_art_pixels.data(), steam_art_width, steam_art_height)) {
      steam_art_width = steam_art_height = 0; steam_art_fallback = true;
    } else steam_art_fallback = false;
  }
  if (steam_art_fallback && !steam_art_width) {
    steam->request_game_cover(app);
    std::vector<std::uint8_t> fallback;
    if (steam->take_game_cover(app, fallback)) {
      if (steam_art_pixels.empty()) steam_art_pixels.resize(image::MaxPixels);
      if (image::read_memory(fallback.data(), fallback.size(), steam_art_pixels.data(), steam_art_width, steam_art_height))
        steam_art_fallback = false;
      else {
        steam_art_width = steam_art_height = 0;
        steam->reject_game_cover(app);
      }
    }
  }
  if (!artwork.logo.empty()) {
    if (steam_logo_pixels.empty()) steam_logo_pixels.resize(image::MaxPixels);
    if (!image::read_memory(artwork.logo.data(), artwork.logo.size(), steam_logo_pixels.data(), steam_logo_width, steam_logo_height))
      steam_logo_width = steam_logo_height = 0;
  }
}
void Shell::explorer_ui(Rect wr) {
  file_window_bounds = wr;
  file_content_bounds = {wr.x + 205, wr.y + 108, wr.w - 225, wr.h - 128};
  steam_list_bounds = {}; steam_scroll_track = {}; steam_home_bounds = {}; steam_home_track = {}; steam_queue_bounds = {}; steam_queue_track = {};
  int x = wr.x, y = wr.y;
  if (file_games) {
    const uint32_t white = 0xfff5ede4, dim = 0xffc0ae9a;
    rect({x + 1, y + 42, wr.w - 2, wr.h - 43}, 0xff30251b);
    rect({x + 1, y + 42, wr.w - 2, 64}, 0xff211a17);
    // Draw the original Steam mark from its unmodified public header asset.
    if (vapor_mark_width == 744 && vapor_mark_height == 171 && !vapor_mark_pixels.empty()) {
      vapor_icon(x + 18, y + 52, 34);
    } else {
      round({x + 18, y + 52, 34, 34}, 17, 0xffc77927);
      line(x + 25, y + 73, x + 34, y + 77, white, 4);
      line(x + 34, y + 77, x + 43, y + 64, white, 4);
    }
    text(x + 59, y + 62, "VAPOR", 1, white);
    if (focus == 965 || hover == 965) rect({x + 175, y + 48, 130, 49}, 0xff3f3227);
    add({x + 175, y + 48, 130, 49}, 965);
    text(x + 192, y + 61, "LIBRARY", 1, library_downloads ? dim : white);
    if (!library_downloads) rect({x + 190, y + 95, 90, 3}, 0xffff9c19);
    Rect footer{x + 1, y + wr.h - 48, wr.w - 2, 47};
    rect(footer, 0xff211a17);
    Rect downloads{x + (wr.w - 230) / 2, footer.y + 4, 230, 38};
    if (library_downloads || focus == 966 || hover == 966) rect(downloads, 0xff3f3227);
    add(downloads, 966);
    const int icon_x = downloads.x + 13, icon_y = downloads.y + 10;
    line(icon_x + 7, icon_y, icon_x + 7, icon_y + 11, dim, 2);
    line(icon_x + 3, icon_y + 7, icon_x + 7, icon_y + 11, dim, 2);
    line(icon_x + 7, icon_y + 11, icon_x + 11, icon_y + 7, dim, 2);
    line(icon_x + 1, icon_y + 15, icon_x + 13, icon_y + 15, dim, 2);
    SteamDownload footer_job;
    if (steam && vapor_transfer_app) for (std::size_t n = 0; n < steam->status().download_count; ++n) {
      SteamDownload candidate;
      if (steam->download(n, candidate) && candidate.app_id == vapor_transfer_app && candidate.state == SteamInstallState::running) { footer_job = candidate; break; }
    }
    if (footer_job.app_id) {
      (void)vapor_game_icon(footer_job.app_id, downloads.x - 12, downloads.y + 7);
      char label[64];
      if (footer_job.total) std::snprintf(label, sizeof(label), "Downloading  %u%%", unsigned(100.0 * double(std::min(footer_job.done, footer_job.total)) / double(footer_job.total)));
      else std::snprintf(label, sizeof(label), "Downloading");
      center(downloads.x + 20, downloads.y + 3, downloads.w - 20, label, 0, white);
      rect({downloads.x + 20, downloads.y + 30, downloads.w - 30, 3}, 0xff4d423b);
      if (footer_job.total) {
        const auto amount = std::min(footer_job.done, footer_job.total);
        rect({downloads.x + 20, downloads.y + 30, int((downloads.w - 30) * (double(amount) / double(footer_job.total))), 3}, 0xfff4a42e);
      }
    } else center(downloads.x, downloads.y + 10, downloads.w, "Manage Downloads", 0, library_downloads ? white : dim);
    if (steam) {
      const auto state = steam->status();
      add({x + wr.w - 350, y + 49, 155, 45}, 970);
      const char *account = state.account_name[0] ? state.account_name : "Steam account";
      text_fit(x + wr.w - 346, y + 62, 145, account, 0, white);
    }
    add({x + wr.w - 180, y + 49, 160, 45}, 400);
    text(x + wr.w - 160, y + 62, "Desktop files", 0, dim);
    if (steam_panel && steam) {
      auto state = steam->status();
      add({x + wr.w - 100, y + 120, 70, 45}, 974);
      text(x + wr.w - 95, y + 130, "Close", 0, white);
      text(x + 45, y + 145, "Steam account", 2, white);
      if (state.account == SteamAccount::online || state.account == SteamAccount::offline) {
        text_fit(x + 45, y + 205, wr.w - 100, state.account_name, 1, white);
        text(x + 45, y + 250, state.account == SteamAccount::online ? "Signed in" : "Offline session", 0, dim);
        if (state.message[0]) text_fit(x + 45, y + 280, wr.w - 100, state.message, 0, dim);
        add({x + 40, y + 315, 160, 48}, 973); rect({x + 40, y + 315, 160, 48}, 0xff604c34);
        center(x + 40, y + 327, 160, "Sign out", 0, white);
      } else if (state.account == SteamAccount::signing_in || state.account == SteamAccount::approval) {
        text(x + 45, y + 205, "Scan with the Steam mobile app to sign in.", 0, white);
        int cell = state.qr_size > 0 ? 360 / (state.qr_size + 8) : 0;
        if (cell >= 2) {
          int left = x + 45, top = y + 250, side = (state.qr_size + 8) * cell;
          rect({left, top, side, side}, 0xffffffff);
          for (int qy = 0; qy < state.qr_size; ++qy)
            for (int qx = 0; qx < state.qr_size; ++qx)
              if (steam->qr_module(qx, qy)) rect({left + (qx + 4) * cell, top + (qy + 4) * cell, cell, cell}, 0xff000000);
        } else if (state.qr_size > 0) {
          text(x + 45, y + 280, "The sign-in code is too large to display.", 0, dim);
        } else text(x + 45, y + 280, "Waiting for Steam...", 0, dim);
        add({x + 500, y + 250, 140, 48}, 972); rect({x + 500, y + 250, 140, 48}, 0xff604c34);
        center(x + 500, y + 262, 140, "Cancel", 0, white);
      } else if (state.account == SteamAccount::signing_out) {
        text(x + 45, y + 205, "Signing out...", 0, dim);
      } else {
        if (state.message[0]) text_fit(x + 45, y + 205, wr.w - 100, state.message, 0, dim);
        add({x + 40, y + 260, 210, 52}, 971); rect({x + 40, y + 260, 210, 52}, 0xff8a692d);
        center(x + 40, y + 274, 210, "Sign in with Steam", 0, white);
        if (state.remember_available) {
          Rect remember{x + 42, y + 335, 340, 42}; add(remember, 983);
          rect({remember.x, remember.y + 7, 22, 22}, state.remember_enabled ? 0xffa9721a : 0xff604c34);
          text(remember.x + 35, remember.y + 10, state.remember_busy ? "Saving choice..." : "Stay signed in", 0, white);
          text(x + 77, y + 385, "Stores a Steam sign-in token on this console.", 0, dim);
        }
      }
      return;
    }
    if (library_downloads) {
      if (!steam || !steam->status().downloads_available) {
        text(x + 42, y + 145, "Downloads", 2, white);
        text(x + 42, y + 205, "Steam downloads are not connected yet.", 0, dim);
        return;
      }
      auto state = steam->status();
      steam_active_download = -1; steam_queue_count = 0;
      for (auto& index : steam_download_rows) index = -1;
      for (auto& job : steam_rendered_downloads) job = {};
      steam_rendered_active = {};
      SteamDownload active;
      for (std::size_t i = 0; i < state.download_count; ++i) {
        SteamDownload job;
        if (steam->download(i, job) && job.state == SteamInstallState::running && steam_active_download < 0) {
          steam_active_download = int(i); active = job;
        }
      }
      rect({x + 1, y + 106, wr.w - 2, 190}, 0xff211a17);
      text(x + 40, y + 126, active.app_id ? "NOW DOWNLOADING" : "DOWNLOADS", 0, dim);
      if (active.app_id) {
        steam_rendered_active = active;
        update_steam_art(active.app_id);
        if (steam_art_width > 0 && steam_art_height > 0 && !steam_art_pixels.empty()) {
          const int left = x + 20, top = y + 148, dw = wr.w / 2 - 45, dh = 130;
          int crop_w = steam_art_width, crop_h = steam_art_height;
          if (static_cast<long long>(crop_w) * dh > static_cast<long long>(crop_h) * dw)
            crop_w = int(static_cast<long long>(crop_h) * dw / dh);
          else crop_h = int(static_cast<long long>(crop_w) * dh / dw);
          const int sx = (steam_art_width - crop_w) / 2, sy = (steam_art_height - crop_h) / 2;
          for (int py = 0; py < dh; ++py) for (int px = 0; px < dw; ++px) {
            const int xx = left + px, yy = top + py;
            if (xx >= 0 && yy >= 0 && xx < W && yy < H)
              pixels[yy * W + xx] = steam_art_pixels[(sy + py * crop_h / dh) * steam_art_width + sx + px * crop_w / dw] | 0xff000000u;
          }
          const int backing_width = std::min(wr.w / 2 - 55, width(active.title, 1) + 59);
          rect({x + 25, y + 151, backing_width, 42}, 0x4c211a17);
        }
        (void)vapor_game_icon(active.app_id, x + 40, y + 158);
        text_fit(x + 72, y + 155, wr.w / 2 - 102, active.title, 1, white);
        char speed[80], peak[80];
        if (active.has_rate) std::snprintf(speed, sizeof(speed), "%.1f Mbps / %.1f MB/s", double(active.bytes_per_second) * 8.0 / 1000000.0, double(active.bytes_per_second) / 1000000.0);
        else std::snprintf(speed, sizeof(speed), "--");
        if (vapor_peak_known) std::snprintf(peak, sizeof(peak), "%.1f Mbps / %.1f MB/s", double(vapor_transfer_peak) * 8.0 / 1000000.0, double(vapor_transfer_peak) / 1000000.0);
        else std::snprintf(peak, sizeof(peak), "--");
        text(x + wr.w / 2, y + 122, "PAYLOAD RATE", 0, dim);
        text_fit(x + wr.w / 2, y + 145, wr.w / 4 - 12, speed, 0, white);
        text(x + wr.w * 3 / 4, y + 122, "PEAK RATE", 0, dim);
        text_fit(x + wr.w * 3 / 4, y + 145, wr.w / 4 - 20, peak, 0, white);
        const auto progress = vapor_download_progress(active.app_id, active);
        if (progress.available) text_fit(x + wr.w / 2, y + 175, wr.w / 2 - 120, progress.text, 0, white);
        else {
          char done[32], unknown[48]; vapor_format_size(done, sizeof(done), active.done);
          std::snprintf(unknown, sizeof(unknown), "%s / --", done);
          text_fit(x + wr.w / 2, y + 175, wr.w / 2 - 120, unknown, 0, white);
        }
        rect({x + wr.w / 2, y + 200, wr.w / 2 - 120, 5}, 0xff504238);
        if (active.total) rect({x + wr.w / 2, y + 200, int((wr.w / 2 - 120) * double(std::min(active.done, active.total)) / double(active.total)), 5}, 0xfff4a42e);
        const auto eta = vapor_download_eta(active);
        if (!eta.empty()) text_fit(x + wr.w / 2, y + 222, wr.w / 2 - 205, eta.c_str(), 0, dim);
        Rect pause{x + wr.w - 95, y + 215, 52, 44}; round(pause, 4, 0xffb5791a); add(pause, 1105); center(pause.x, pause.y + 10, pause.w, "II", 0, white);
        if (steam->can_cancel_download()) {
          Rect cancel{x + wr.w - 190, y + 215, 85, 44}; add(cancel, 1125); center(cancel.x, cancel.y + 10, cancel.w, "Cancel", 0, dim);
        }
        const int graph_x = x + 40, graph_y = y + 225, graph_w = wr.w / 2 - 90;
        rect({graph_x - 5, graph_y - 26, graph_w + 10, 78}, 0xe0211a17);
        text(graph_x, graph_y - 22, "RECENT PAYLOAD RATE", 0, dim);
        rect({graph_x, graph_y + 47, graph_w, 1}, 0xff7a6858);
        if (vapor_rate_samples && vapor_transfer_peak) for (int i = 0; i < vapor_rate_samples; ++i) {
          if (!vapor_rate_known[i]) continue;
          int height = int(48.0 * double(vapor_rate_history[i]) / double(vapor_transfer_peak));
          if (height > 48) height = 48;
          const int px = graph_x + i * graph_w / 60;
          rect({px, graph_y + 48 - height, std::max(1, graph_w / 75), height}, 0xfff4a42e);
        }
      } else text(x + 40, y + 165, "No active downloads", 1, dim);
      for (std::size_t i = 0; i < state.download_count; ++i) if (int(i) != steam_active_download) ++steam_queue_count;
      const int max_offset = std::max(0, steam_queue_count - 5);
      if (steam_download_offset > max_offset) steam_download_offset = max_offset;
      steam_queue_bounds = {x + 35, y + 355, wr.w - 70, 325};
      char heading[64]; std::snprintf(heading, sizeof(heading), "Other downloads (%d)", steam_queue_count);
      text(x + 40, y + 320, heading, 1, white);
      if (!steam_queue_count) text(x + 40, y + 360, state.message[0] ? state.message : "There are no other downloads.", 0, dim);
      int logical = 0, shown = 0;
      for (std::size_t i = 0; i < state.download_count && shown < 5; ++i) {
        if (int(i) == steam_active_download) continue;
        if (logical++ < steam_download_offset) continue;
        steam_download_rows[shown++] = int(i);
      }
      for (int row = 0; row < 5; ++row) {
        SteamDownload job; const int index = steam_download_rows[row];
        if (index < 0 || !steam->download(std::size_t(index), job)) break;
        steam_rendered_downloads[row] = job;
        int top = y + 355 + row * 65;
        rect({x + 35, top, wr.w - 70, 59}, 0xff3b3027);
        round({x + 45, top + 7, 45, 45}, 3, 0xff69513a);
        if (!vapor_game_icon(job.app_id, x + 56, top + 18)) {
          char initial[2] = {job.title[0] ? job.title[0] : '?', 0}; center(x + 45, top + 17, 45, initial, 1, white);
        }
        text_fit(x + 105, top + 5, wr.w - 505, job.title, 0, white);
        const auto progress = vapor_download_row_status(job);
        text_fit(x + 105, top + 30, wr.w - 505, progress.c_str(), 0, dim);
        bool pause = job.state == SteamInstallState::running || job.state == SteamInstallState::queued;
        if (!job.cleanup_failure || (job.cleanup_retryable && steam->can_cancel_download())) {
          add({x + wr.w - 350, top + 7, 115, 46}, 1100 + row);
          center(x + wr.w - 350, top + 19, 115, job.cleanup_failure ? "Retry cleanup" : job.state == SteamInstallState::installed ? "Clear" : pause ? "Pause" : "Resume", 0, white);
        }
        if (job.state != SteamInstallState::installed && !job.cleanup_failure && steam->can_cancel_download()) {
          add({x + wr.w - 220, top + 7, 160, 46}, 1120 + row);
          center(x + wr.w - 220, top + 19, 160, "Cancel", 0, white);
        }
      }
      if (steam_queue_count > 5) {
        steam_queue_track = {x + wr.w - 22, steam_queue_bounds.y, 10, steam_queue_bounds.h};
        rect(steam_queue_track, 0xff211c18);
        const int thumb = std::max(24, steam_queue_track.h * 5 / steam_queue_count);
        const int thumb_y = steam_queue_track.y + (steam_queue_track.h - thumb) * steam_download_offset / max_offset;
        round({steam_queue_track.x, thumb_y, steam_queue_track.w, thumb}, 4, 0xff7a6858);
      }
      text(x + 340, y + 698, steam->can_cancel_download() ? "Cancel removes partial download files." : "Cancellation is unavailable. Pause keeps files.", 0, dim);
      return;
    }
    if (steam) {
      auto state = steam->status();
      if (state.account == SteamAccount::online || state.account == SteamAccount::offline) {
        Rect list{x + 1, y + 106, 292, wr.h - 155}; rect(list, 0xff30251e);
        steam_list_bounds = list;
        Rect home{list.x + 8, list.y + 10, list.w - 16, 34};
        if (steam_selection < 0 || focus == 985 || hover == 985) rect(home, 0xff504238);
        add(home, 985); text(home.x + 12, home.y + 7, "Home", 0, white);
        std::uint64_t signature = 14695981039346656037ull;
        auto hash = [&](std::uint64_t value) { signature = (signature ^ value) * 1099511628211ull; };
        hash(state.game_count);
        hash(vapor_favorites_only); hash(vapor_home_installed_only); hash(vapor_favorites_collapsed); hash(vapor_home_recent_first);
        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(vapor_search); *c; ++c) hash(*c);
        // Filters and discovery of additional games update immediately. Stable
        // library content is checked at most four times a second by the native
        // provider; mouse/controller rendering keeps its normal frame rate.
        for (std::size_t i = 0; i < vapor_shortcuts.count(); ++i) {
          hash(vapor_shortcuts.entries()[i].app); hash(vapor_shortcuts.entries()[i].placements);
        }
        const auto context = signature;
        const auto now_ms = std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        if (context != steam_rows_context || now_ms >= steam_rows_next_scan_ms) {
          steam_rows_context = context;
          steam_rows_next_scan_ms = now_ms + steam->library_poll_interval_ms();
          for (std::size_t i = 0; i < state.game_count; ++i) {
            SteamGame item;
            if (!steam->game_summary(i, item)) { hash(0); continue; }
            hash(item.app_id); hash(static_cast<unsigned>(item.install));
            hash(item.has_last_played); hash(item.last_played_unix_seconds);
            hash(vapor_shortcuts.placements(item.app_id));
            for (const unsigned char* c = reinterpret_cast<const unsigned char*>(item.title); *c; ++c) hash(*c);
          }
          if (signature != steam_rows_signature) {
            steam_rows.clear(); std::size_t favorites = 0;
            std::vector<std::pair<std::string, int>> ordered_games;
            ordered_games.reserve(state.game_count);
            std::string search_key(vapor_search);
            for (char& c : search_key) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
            for (std::size_t i = 0; i < state.game_count; ++i) {
              SteamGame item;
              if (!steam->game_summary(i, item)) continue;
              const bool favorite = (vapor_shortcuts.placements(item.app_id) & VaporShortcuts::Favorite) != 0;
              if (favorite) ++favorites;
              const bool installed = item.install == SteamInstallState::installed || item.install == SteamInstallState::unavailable;
              if ((!vapor_favorites_only || favorite) && (!vapor_home_installed_only || installed)) {
                std::string sort_key(item.title);
                for (char& c : sort_key) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
                if (search_key.empty() || sort_key.find(search_key) != std::string::npos)
                  ordered_games.emplace_back(std::move(sort_key), int(i));
              }
            }
            std::sort(ordered_games.begin(), ordered_games.end(), [](const auto& a, const auto& b) {
              return a.first == b.first ? a.second < b.second : a.first < b.first;
            });
            for (const auto& entry : ordered_games) steam_rows.push_back(entry.second);
            // Keep Home's All Games data unique. The sidebar has a separate visual
            // Favorites section, followed by the complete alphabetical library.
            steam_library_rows.clear();
            if (!vapor_favorites_only && !vapor_favorites_collapsed && favorites) {
              for (int index : steam_rows) {
                SteamGame game;
                if (steam->game_summary(std::size_t(index), game) &&
                    (vapor_shortcuts.placements(game.app_id) & VaporShortcuts::Favorite))
                  steam_library_rows.push_back(index);
              }
              if (!steam_library_rows.empty()) steam_library_rows.push_back(-1); // All Games divider.
            }
            steam_library_rows.insert(steam_library_rows.end(), steam_rows.begin(), steam_rows.end());
            steam_home_rows.clear();
            std::vector<std::pair<int, std::uint64_t>> home_order;
            for (int index : steam_rows) {
              SteamGame game;
              if (!steam->game_summary(std::size_t(index), game)) continue;
              if (!vapor_home_installed_only || game.install == SteamInstallState::installed ||
                  game.install == SteamInstallState::unavailable) {
                const auto played = game.has_last_played && game.last_played_unix_seconds > 0 &&
                    game.last_played_unix_seconds < 4102444800ULL ? game.last_played_unix_seconds : 0;
                home_order.emplace_back(index, played);
              }
            }
            if (vapor_home_recent_first)
              std::stable_sort(home_order.begin(), home_order.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            for (const auto& entry : home_order) steam_home_rows.push_back(entry.first);
            steam_recent_games.clear();
            for (int index : steam_rows) {
              SteamGame game;
              if (steam->game_summary(std::size_t(index), game) && game.app_id && game.has_last_played &&
                  game.last_played_unix_seconds > 0 && game.last_played_unix_seconds < 4102444800ULL)
                steam_recent_games.push_back({game.last_played_unix_seconds, game.app_id, index});
            }
            std::sort(steam_recent_games.begin(), steam_recent_games.end(), [](const SteamRecentGame &a, const SteamRecentGame &b) {
              return a.played == b.played ? a.app < b.app : a.played > b.played;
            });
            steam_favorite_count = favorites;
            steam_rows_signature = signature;
          }
        }
        const auto favorites = steam_favorite_count;
        Rect favorite_filter{list.x + 8, list.y + 46, list.w - 54, 28}; add(favorite_filter, 1150);
        if (vapor_favorites_only || hover == 1150 || focus == 1150) rect(favorite_filter, 0xff504238);
        char favorite_label[64]; std::snprintf(favorite_label, sizeof(favorite_label), "FAVORITES (%zu)", favorites);
        text(list.x + 20, list.y + 50, favorite_label, 0, white);
        if (!vapor_favorites_only) {
          Rect toggle{list.x + list.w - 42, list.y + 46, 28, 28}; add(toggle, 1045);
          if (hover == 1045 || focus == 1045) rect(toggle, 0xff504238);
          center(toggle.x, toggle.y + 3, toggle.w, vapor_favorites_collapsed ? "+" : "-", 0, dim);
        }
        text(list.x + 20, list.y + 80, vapor_favorites_only ? "FAVORITE GAMES" : "GAMES AND SOFTWARE", 0, white);
        Rect installed_filter{list.x + 190, list.y + 102, 84, 27}; add(installed_filter, 1046);
        round(installed_filter, 3, (focus == 1046 || hover == 1046) ? 0xff67899b :
              vapor_home_installed_only ? 0xff496777 : 0xff504238);
        center(installed_filter.x, installed_filter.y + 5, installed_filter.w, "INSTALLED", 0, white);
        if (state.library == SteamLibrary::loading) text(list.x + 20, list.y + 106, "Loading...", 0, dim);
        else if (state.message[0]) text_fit(list.x + 20, list.y + 106, 160, state.message, 0, dim);
        else { char count[60]; std::snprintf(count, sizeof(count), "%zu games", steam_rows.size()); text(list.x + 20, list.y + 106, count, 0, dim); }
        Rect search_box{list.x + 8, list.y + 130, list.w - 30, 32};
        round(search_box, 3, 0xff211c18); add(search_box, 1152);
        text_fit(search_box.x + 8, search_box.y + 6, search_box.w - 44,
                 vapor_search[0] ? vapor_search : "Search games", 0, vapor_search[0] ? white : dim);
        if (text_target == TextTarget::vapor_search && text_caret >= 0 && text_caret <= int(std::strlen(vapor_search))) {
          const std::string prefix(vapor_search, std::size_t(text_caret));
          const int caret_x = width(prefix.c_str(), 0);
          if (caret_x < search_box.w - 44) rect({search_box.x + 8 + caret_x, search_box.y + 5, 2, 22}, 0xfff4a42e);
        }
        if (vapor_search[0]) { Rect clear{search_box.x + search_box.w - 30, search_box.y, 30, 32}; add(clear, 1153); center(clear.x, clear.y + 6, clear.w, "X", 0, dim); }
        int rows = (list.h - 235) / 32;
        steam_visible_rows = rows < 1 ? 1 : rows > 20 ? 20 : rows;
        const auto remaining = steam_library_rows.size() > std::size_t(steam_visible_rows) ? steam_library_rows.size() - steam_visible_rows : 0;
        if (std::size_t(steam_scroll) > remaining) steam_scroll = int(remaining);
        for (auto &app : steam_rendered_rows) app = 0;
        if (steam_rows.empty() && (vapor_home_installed_only || vapor_search[0] || vapor_favorites_only))
          text_fit(list.x + 20, list.y + 183, list.w - 50, "No games match your filters.", 0, dim);
        for (int row = 0; row < steam_visible_rows; ++row) {
          int index = steam_scroll + row; SteamGame game;
          if (std::size_t(index) >= steam_library_rows.size()) break;
          index = steam_library_rows[index];
          if (index < 0) {
            text_fit(list.x + 20, list.y + 183 + row * 32, list.w - 50, "ALL GAMES", 0, dim);
            continue;
          }
          if (!steam->game_summary(std::size_t(index), game)) continue;
          steam_rendered_rows[row] = game.app_id;
          Rect item{list.x + 8, list.y + 180 + row * 32, list.w - 30, 30};
          if (steam_selection == index) rect(item, 0xff665544);
          else if (focus == 1000 + row || hover == 1000 + row) rect(item, 0xff504238);
          add(item, 1000 + row);
          if (!vapor_game_icon(game.app_id, item.x + 5, item.y + 4)) {
            round({item.x + 5, item.y + 4, 22, 22}, 3, 0xff594d3b);
            char initial[2] = {game.title[0] ? game.title[0] : '?', 0};
            text(item.x + 9, item.y + 6, initial, 0, white);
          }
          text_fit(item.x + 34, item.y + 6, item.w - 40, game.title, 0, white);
        }
        steam_scroll_track = {list.x + list.w - 17, list.y + 180, 10, steam_visible_rows * 32 - 3};
        rect(steam_scroll_track, 0xff211c18);
        int thumb = !steam_library_rows.empty() ? int(static_cast<long long>(steam_scroll_track.h) * steam_visible_rows / steam_library_rows.size()) : steam_scroll_track.h;
        if (thumb < 30) thumb = 30;
        if (thumb > steam_scroll_track.h) thumb = steam_scroll_track.h;
        int thumb_y = steam_scroll_track.y;
        if (remaining) thumb_y += int(static_cast<long long>(steam_scroll_track.h - thumb) * steam_scroll / remaining);
        round({steam_scroll_track.x, thumb_y, steam_scroll_track.w, thumb}, 4, 0xff7a6858);
        add({list.x + 12, list.y + list.h - 53, 140, 42}, 975); text(list.x + 20, list.y + list.h - 43, "Refresh library", 0, dim);
        int dx = x + 293, dw = wr.w - 294;
        rect({dx, y + 106, dw, 230}, 0xff62472b);
        rect({dx, y + 220, dw, 116}, 0xff493624);
        SteamGame selected;
        if (steam_selection >= 0 && steam->game(std::size_t(steam_selection), selected)) {
          update_steam_art(selected.app_id);
          if (steam_art_width > 0 && steam_art_height > 0 && !steam_art_pixels.empty()) {
            // Fill the wide game header without stretching the source artwork.
            const int dh = 230;
            int crop_w = steam_art_width, crop_h = steam_art_height;
            if (static_cast<long long>(crop_w) * dh > static_cast<long long>(crop_h) * dw)
              crop_w = int(static_cast<long long>(crop_h) * dw / dh);
            else crop_h = int(static_cast<long long>(crop_w) * dh / dw);
            const int sx = (steam_art_width - crop_w) / 2, sy = (steam_art_height - crop_h) / 2;
            for (int py = 0; py < dh; ++py) {
              const int dest_y = y + 106 + py;
              if (dest_y < 0 || dest_y >= H) continue;
              const int source_y = sy + int(static_cast<long long>(py) * crop_h / dh);
              for (int px = 0; px < dw; ++px) {
                const int dest_x = dx + px;
                if (dest_x < 0 || dest_x >= W) continue;
                const int source_x = sx + int(static_cast<long long>(px) * crop_w / dw);
                pixels[dest_y * W + dest_x] = steam_art_pixels[source_y * steam_art_width + source_x] | 0xff000000u;
              }
            }
            if (!steam_logo_width) rect({dx + 20, y + 160, dw - 40, 52}, 0x990e1319);
          }
          if (steam_art_width && steam_logo_width && steam_logo_height) {
            int logo_w = dw / 2, logo_h = int(static_cast<long long>(logo_w) * steam_logo_height / steam_logo_width);
            if (logo_h > 150) { logo_h = 150; logo_w = int(static_cast<long long>(logo_h) * steam_logo_width / steam_logo_height); }
            for (int py = 0; py < logo_h; ++py) {
              const int yy = y + 140 + py; if (yy < 0 || yy >= H) continue;
              const int sy = int(static_cast<long long>(py) * steam_logo_height / logo_h);
              for (int px = 0; px < logo_w; ++px) {
                const int xx = dx + 32 + px; if (xx < 0 || xx >= W) continue;
                const int sx = int(static_cast<long long>(px) * steam_logo_width / logo_w);
                const auto color = steam_logo_pixels[sy * steam_logo_width + sx];
                const unsigned a = color >> 24;
                if (!a) continue;
                auto& destination = pixels[yy * W + xx];
                if (a == 255) destination = color;
                else {
                  std::uint32_t blended = 0xff000000u;
                  for (int shift = 0; shift < 24; shift += 8)
                    blended |= (((color >> shift & 255) * a + (destination >> shift & 255) * (255 - a)) / 255) << shift;
                  destination = blended;
                }
              }
            }
          } else text_fit(dx + 32, y + 175, dw - 64, selected.title, 1, white);
          rect({dx, y + 336, dw, 84}, 0xff30261e);
          Rect heart{dx + dw - 65, y + 352, 42, 42}; add(heart, 1151);
          if (hover == 1151 || focus == 1151) round(heart, 4, 0xff504238);
          const uint32_t heart_color = (vapor_shortcuts.placements(selected.app_id) & VaporShortcuts::Favorite) ? 0xfff0a020 : dim;
          round({heart.x + 8, heart.y + 10, 12, 12}, 6, heart_color);
          round({heart.x + 18, heart.y + 10, 12, 12}, 6, heart_color);
          for (int n = 0; n < 12; ++n) line(heart.x + 8 + n, heart.y + 18 + n, heart.x + 30 - n, heart.y + 18 + n, heart_color);
          steam_detail_job_app = 0;
          VaporDownloadProgress selected_progress;
          if (selected.install == SteamInstallState::queued || selected.install == SteamInstallState::running ||
              selected.install == SteamInstallState::paused) {
            for (std::size_t i = 0; i < state.download_count; ++i) {
              SteamDownload download;
              if (steam->download(i, download)) {
                selected_progress = vapor_download_progress(selected.app_id, download);
                if (selected_progress.available) break;
              }
            }
          }
          bool existing = false;
          for (int i = 0; i < game_inventory.count; ++i)
            if (selected.app_id && game_inventory.entries[i].steam_app == selected.app_id) existing = true;
          if (selected.can_play) {
            Rect play{dx + 30, y + 351, 150, 54};
            round(play, 4, selected.activity == SteamGameActivity::registration_required ? 0xffb5791a : 0xff2b9c66);
            const char* label = "PLAY";
            if (selected.activity == SteamGameActivity::registration_required) { label = "FINISH SETUP"; add(play, 986); }
            else if (selected.activity == SteamGameActivity::preparing) label = "PREPARING";
            else if (selected.activity == SteamGameActivity::starting) label = "STARTING";
            else if (selected.activity == SteamGameActivity::playing) label = "PLAYING";
            else add(play, 979);
            center(play.x, play.y + 15, play.w, label, 1, white);
          } else if (selected.install == SteamInstallState::running || selected.install == SteamInstallState::queued ||
                     selected.install == SteamInstallState::paused) {
            const bool paused = selected.install == SteamInstallState::paused;
            const int action_id = paused ? 1049 : 1048;
            Rect action{dx + 30, y + 351, 150, 54};
            steam_detail_job_app = selected.app_id; add(action, action_id);
            round(action, 4, (focus == action_id || hover == action_id) ? 0xffe89b27 : 0xffb5791a);
            center(action.x, action.y + 15, action.w, paused ? "RESUME" : "PAUSE", 1, white);
            if (!selected_progress.available)
              text_fit(dx + 200, y + 365, dw - 280, paused ? "Download paused" :
                       selected.install == SteamInstallState::queued ? "Download queued" : "Downloading", 0, dim);
          } else if (selected.app_id == 1089130 && existing) {
            Rect play{dx + 30, y + 351, 150, 54}; round(play, 4, 0xff2b9c66); add(play, 30);
            center(play.x, play.y + 15, play.w, "PLAY", 1, white);
          } else if (existing) {
            text_fit(dx + 32, y + 365, selected_progress.available ? 150 : dw - 110, "Existing game record - launch not connected yet", 0, dim);
          } else if (state.downloads_available && selected.install == SteamInstallState::not_installed && state.account == SteamAccount::online) {
            Rect install{dx + 30, y + 351, 150, 54}; round(install, 4, 0xffb5791a); add(install, 978);
            center(install.x, install.y + 15, install.w, "INSTALL", 1, white);
          } else if (selected.install == SteamInstallState::failed) {
            Rect retry{dx + 30, y + 351, 150, 54}; round(retry, 4, 0xffb5791a); add(retry, 984);
            center(retry.x, retry.y + 15, retry.w, "RETRY", 1, white);
            text_fit(dx + 195, y + 365, dw - 275, selected.detail[0] ? selected.detail : "Download failed", 0, dim);
          } else {
            const char* label = selected.install == SteamInstallState::installed ? "Installed - launch not connected yet"
              : selected.install == SteamInstallState::unavailable ? "Installed - storage unavailable"
              : selected.install == SteamInstallState::queued ? "Download queued"
              : selected.install == SteamInstallState::running ? "Downloading"
              : selected.install == SteamInstallState::paused ? "Download paused"
              : selected.install == SteamInstallState::failed ? "Download failed"
              : "Install and launch not connected yet";
            text_fit(dx + 32, y + 365, selected_progress.available ? 150 : dw - 110, label, 0, dim);
          }
          if (selected_progress.available && !selected.can_play)
            text_fit(dx + 200, y + 365, dw - 280, selected_progress.text, 0, white);
          // Per-game controller layout: a compact orange control under the facts
          // row, right of PLAY. The content below is laid out 25px lower for it.
          Rect refresh_cache{dx + 30, y + 412, 158, 32};
          add(refresh_cache, 1172);
          round(refresh_cache, 4, (hover == 1172 || focus == 1172) ? 0xff74521d : 0xff594534);
          center(refresh_cache.x, refresh_cache.y + 7, refresh_cache.w, "Refresh game cache", 0, white);
          SteamControls layout{};
          if (selected.can_play && steam->controls(selected.app_id, layout)) {
            Rect controls{dx + 200, y + 410, 260, 32};
            steam_controls_app = selected.app_id; add(controls, 1050);
            const bool lit = focus == 1050 || hover == 1050;
            round(controls, 4, lit ? 0xffe8851e : 0xcce8851e);
            char label_text[64];
            std::snprintf(label_text, sizeof(label_text), "CONTROLS: %s", steam_controls_name(layout));
            center(controls.x, controls.y + 6, controls.w, label_text, 0, white);
            if (!selected.can_manage_install && !selected.management_reason[0])
              text_fit(controls.x + controls.w + 14, y + 416, dw - 520, steam_controls_hint(layout), 0, dim);
          }
          if (selected.can_manage_install && selected.install == SteamInstallState::installed &&
              selected.activity == SteamGameActivity::idle) {
            Rect remove{dx + dw - 335, y + 414, 135, 30}, reinstall{dx + dw - 190, y + 414, 150, 30};
            add(remove, 1170); if (selected.can_reinstall) add(reinstall, 1171);
            round(remove, 4, 0xff594534); if (selected.can_reinstall) round(reinstall, 4, 0xff74521d);
            center(remove.x, remove.y + 5, remove.w, "REMOVE", 0, white);
            if (selected.can_reinstall) center(reinstall.x, reinstall.y + 5, reinstall.w, "REINSTALL", 0, white);
          } else if (selected.management_reason[0]) {
            text_fit(dx + dw - 335, y + 420, 300, selected.management_reason, 0, dim);
          }
          if (selected.can_play || selected.install == SteamInstallState::not_installed) {
            // Keep download size distinct from space needed on disk. Steam
            // supplies these independently, so omit either when unavailable.
            const VaporGameFacts facts = vapor_game_facts(selected);
            const int facts_x = dx + 200;
            const int fact_w = (dw - 280) / 4;
            if (fact_w > 0) {
              auto fact = [&](int column, const char *label, const char *value) {
                const int left = facts_x + column * fact_w;
                text_fit(left, y + 351, fact_w - 8, label, 0, dim);
                text_fit(left, y + 375, fact_w - 8, value, 0, white);
              };
              if (facts.install_size_available) fact(0, vapor_install_size_label(selected), facts.install_size);
              if (facts.download_size_available) fact(1, "DOWNLOAD SIZE", facts.download_size);
              if (selected.has_last_played && selected.last_played_unix_seconds > 0 && selected.last_played_unix_seconds < 4102444800ULL) {
                char last_played[32];
                if (steam_date(selected.last_played_unix_seconds, last_played, sizeof(last_played)))
                  fact(2, "LAST PLAYED", last_played);
              }
              if (facts.playtime_available) fact(3, "PLAY TIME", facts.playtime);
            }
          }
        } else {
          steam_art_app = 0; steam_art_width = steam_art_height = 0; steam_art_fallback = false; steam_logo_width = steam_logo_height = 0;
          steam_description.clear(); steam_genres.clear();
          steam_news_state = SteamNewsState::pending; steam_news_title.clear(); steam_news_feed.clear(); steam_news_description.clear(); steam_news_published = 0;
          const auto& recent = steam_recent_games;
          steam_recent_count = int(recent.size());
          steam_recent_offset = std::max(0, std::min(steam_recent_offset, std::max(0, steam_recent_count - 3)));
          for (auto &app : steam_rendered_recent) app = 0;
          if (!recent.empty()) {
            const int shelf_x = dx + 25, shelf_w = dw - 80;
            text(shelf_x, y + 116, "RECENTLY PLAYED", 0, dim);
            if (steam_recent_offset > 0) { Rect prev{dx + dw - 76, y + 110, 28, 30}; add(prev, 1043); center(prev.x, prev.y + 4, prev.w, "<", 0, white); }
            if (steam_recent_offset + 3 < steam_recent_count) { Rect next{dx + dw - 42, y + 110, 28, 30}; add(next, 1044); center(next.x, next.y + 4, next.w, ">", 0, white); }
            const int recent_w = shelf_w / 3;
            for (int card = 0; card < 3 && steam_recent_offset + card < steam_recent_count; ++card) {
              const auto &entry = recent[std::size_t(steam_recent_offset + card)];
              SteamGame game; if (!steam->game_summary(std::size_t(entry.index), game) || game.app_id != entry.app) continue;
              Rect tile{shelf_x + card * (recent_w + 15), y + 147, recent_w, 168};
              steam_rendered_recent[card] = entry.app; round(tile, 4, 0xff503a29); add(tile, 1040 + card);
              (void)vapor_game_cover(entry.app, tile);
              rect({tile.x, tile.y + tile.h - 42, tile.w, 42}, 0xe0211a17);
              text_fit(tile.x + 8, tile.y + tile.h - 37, tile.w - 16, game.title, 0, white);
              char played[32]; if (steam_date(entry.played, played, sizeof(played)))
                text_fit(tile.x + 8, tile.y + tile.h - 18, tile.w - 16, played, 0, dim);
            }
          } else {
            text(dx + 32, y + 145, "LIBRARY HOME", 1, white);
            text(dx + 32, y + 195, "Your games", 2, white);
          }
          rect({dx, y + 336, dw, wr.h - 385}, 0xff30251e);
          const int home_notice_height = state.message[0] ? 38 : 0;
          if (home_notice_height) {
            rect({dx + 20, y + 342, dw - 40, 32}, 0xff493b2b);
            text_fit(dx + 30, y + 348, dw - 60, state.message, 0, white);
          }
          text(dx + 32, y + 365 + home_notice_height, "ALL GAMES", 0, dim);
          Rect sort_button{dx + dw - 200, y + 355 + home_notice_height, 170, 30};
          round(sort_button, 3, 0xff504238); add(sort_button, 1047);
          center(sort_button.x, sort_button.y + 6, sort_button.w,
                 vapor_home_recent_first ? "SORT: RECENT" : "SORT: A-Z", 0, white);
          if (steam_home_rows.empty() && (vapor_search[0] || vapor_home_installed_only))
            text(dx + 32, y + 415 + home_notice_height,
                 vapor_home_installed_only ? "No installed games match your filters." : "No games match your search.", 0, dim);
          steam_home_bounds = {dx, y + 400 + home_notice_height, dw, wr.h - 450 - home_notice_height};
          steam_home_columns = std::max(3, std::min(6, (dw - 70) / 185));
          const int card_w = std::min((dw - 55 - (steam_home_columns - 1) * 12) / steam_home_columns,
                                      std::max(1, (steam_home_bounds.h - 10) * 2 / 3));
          const int card_h = card_w * 3 / 2;
          const int total_rows = int((steam_home_rows.size() + steam_home_columns - 1) / steam_home_columns);
          const int max_row = std::max(0, total_rows - 1);
          if (steam_home_row > max_row) steam_home_row = max_row;
          for (int& item : steam_home_items) item = -1;
          for (auto &app : steam_rendered_cards) app = 0;
          for (int card = 0; card < steam_home_columns; ++card) {
            SteamGame game;
            const int row_index = steam_home_row * steam_home_columns + card;
            if (std::size_t(row_index) >= steam_home_rows.size()) break;
            const int game_index = steam_home_rows[row_index];
            if (!steam->game_summary(std::size_t(game_index), game)) continue;
            steam_home_items[card] = game_index;
            steam_rendered_cards[card] = game.app_id;
            Rect tile{dx + 25 + card * (card_w + 12), y + 405 + home_notice_height, card_w, card_h};
            round(tile, 4, 0xff503a29); add(tile, 1030 + card);
            if (!vapor_game_portrait(game.app_id, tile))
              text_fit(tile.x + 12, tile.y + tile.h / 2, tile.w - 24, game.title, 0, white);
          }
          if (total_rows > 1 && steam_home_bounds.h > 0) {
            Rect track{dx + dw - 17, steam_home_bounds.y, 10, steam_home_bounds.h};
            steam_home_track = track;
            rect(track, 0xff211c18);
            int thumb = std::max(24, track.h / total_rows);
            if (thumb > track.h) thumb = track.h;
            const int thumb_y = track.y + (track.h - thumb) * steam_home_row / max_row;
            round({track.x, thumb_y, track.w, thumb}, 3, 0xff7a6858);
          }
        }
        // Status belongs below the action bar; it must not replace the
        // independently loaded game description or real news card.
        if (selected.app_id && selected.activity == SteamGameActivity::registration_required)
          text_fit(dx + 32, y + 454, dw - 64, state.message[0] ? state.message : "Finish Setup completes PS5 registration. Then reopen Vapor and press Play.", 0, dim);
        else if (selected.app_id && state.message[0]) text_fit(dx + 32, y + 454, dw - 64, state.message, 0, dim);
        else if (selected.app_id && (selected.can_manage_install || selected.management_reason[0])) {
          SteamControls hint{};
          if (selected.can_play && steam->controls(selected.app_id, hint))
            text_fit(dx + 32, y + 454, dw - 64, steam_controls_hint(hint), 0, dim);
        }
        if (selected.app_id && !state.cloud_choice_app && !state.cloud_skip_app) {
          const bool has_news = steam_news_state == SteamNewsState::available && !steam_news_title.empty();
          const bool has_about = !steam_description.empty() || !steam_genres.empty();
          const int content_w = dw - 64;
          const int about_w = has_news && has_about ? (content_w - 24) / 2 : content_w;
          if (has_about && about_w > 0) {
            const int about_x = dx + 32, inner_x = about_x + 14, inner_w = about_w - 28;
            round({about_x, y + 479, about_w, 146}, 4, 0xff493b2b);
            text_fit(inner_x, y + 493, inner_w, "ABOUT THIS GAME", 0, dim);
            std::string remaining = steam_description;
            for (int row = 0; row < 3 && !remaining.empty(); ++row) {
              std::size_t end = remaining.size();
              while (end && width(remaining.substr(0, end).c_str(), 0) > inner_w)
                end = vapor_utf8_boundary(remaining, end - 1);
              if (!end) break;
              if (end < remaining.size() && row < 2) {
                const auto space = remaining.rfind(' ', end);
                if (space != std::string::npos && space) end = space;
              }
              const auto line = remaining.substr(0, end);
              if (row == 2 && end < remaining.size())
                text_fit(inner_x, y + 518 + row * 23, inner_w, remaining.c_str(), 0, white);
              else text(inner_x, y + 518 + row * 23, line.c_str(), 0, white);
              remaining.erase(0, end);
              while (!remaining.empty() && remaining.front() == ' ') remaining.erase(0, 1);
            }
            if (!steam_genres.empty()) {
              std::string genres = "GENRES: " + steam_genres;
              text_fit(inner_x, y + 595, inner_w, genres.c_str(), 0, dim);
            }
          }
          if (has_news) {
            const int news_x = dx + 32 + (has_about ? about_w + 24 : 0);
            const int news_w = content_w - (has_about ? about_w + 24 : 0);
            if (news_w > 32) {
              round({news_x, y + 479, news_w, 146}, 4, 0xff493b2b);
              text_fit(news_x + 14, y + 493, news_w - 28, "LATEST NEWS", 0, dim);
              text_fit(news_x + 14, y + 518, news_w - 28, steam_news_title.c_str(), 0, white);
              if (!steam_news_description.empty()) {
                std::string remaining = steam_news_description;
                std::size_t end = remaining.size();
                while (end && width(remaining.substr(0, end).c_str(), 0) > news_w - 28)
                  end = vapor_utf8_boundary(remaining, end - 1);
                if (end && end < remaining.size()) {
                  const auto space = remaining.rfind(' ', end);
                  if (space != std::string::npos && space) end = space;
                }
                if (end) {
                  const auto first = remaining.substr(0, end);
                  text(news_x + 14, y + 545, first.c_str(), 0, dim);
                  remaining.erase(0, end);
                  while (!remaining.empty() && remaining.front() == ' ') remaining.erase(0, 1);
                  if (!remaining.empty()) text_fit(news_x + 14, y + 568, news_w - 28, remaining.c_str(), 0, dim);
                }
              }
              if (!steam_news_feed.empty())
                text_fit(news_x + 14, y + 595, (news_w - 32) / 2, steam_news_feed.c_str(), 0, dim);
              if (steam_news_published > 0 && steam_news_published < 4102444800ULL) {
                char published[32];
                if (steam_date(steam_news_published, published, sizeof(published))) {
                  const int date_x = steam_news_feed.empty() ? news_x + 14 : news_x + news_w / 2;
                  text_fit(date_x, y + 595, news_x + news_w - 14 - date_x, published, 0, dim);
                }
              }
            }
          }
        }
        // Keep real play history visible when the action bar is occupied by
        // download progress, a failure, or unavailable storage.
        if (selected.app_id && !selected.can_play && selected.install != SteamInstallState::not_installed &&
            !state.cloud_choice_app && !state.cloud_skip_app) {
          char history[160]{};
          char date[32]{};
          if (selected.has_last_played && selected.last_played_unix_seconds > 0 &&
              selected.last_played_unix_seconds < 4102444800ULL &&
              steam_date(selected.last_played_unix_seconds, date, sizeof(date)))
            std::snprintf(history, sizeof(history), "LAST PLAYED: %s", date);
          if (selected.has_playtime) {
            const auto facts = vapor_game_facts(selected);
            const auto used = std::strlen(history);
            std::snprintf(history + used, sizeof(history) - used, "%sPLAY TIME: %s", used ? "   |   " : "", facts.playtime);
          }
          if (history[0]) text_fit(dx + 32, y + 631, dw - 64, history, 0, dim);
        }
        if (selected.app_id && !state.cloud_choice_app && !state.cloud_skip_app) {
          const auto placed = vapor_shortcuts.placements(selected.app_id);
          const char* labels[] = {placed & VaporShortcuts::Desktop ? "REMOVE DESKTOP" : "ADD TO DESKTOP",
                                  placed & VaporShortcuts::Start ? "UNPIN START" : "PIN TO START",
                                  placed & VaporShortcuts::Taskbar ? "UNPIN TASKBAR" : "PIN TO TASKBAR"};
          for (int i = 0; i < 3; ++i) {
            Rect action{dx + 30 + i * 205, y + 655, 190, 42};
            round(action, 4, 0xff504238); add(action, 1160 + i);
            center(action.x, action.y + 10, action.w, labels[i], 0, white);
          }
        }
        if (selected.app_id && selected.app_id == state.cloud_choice_app) {
          text_fit(dx + 32, y + 505, dw - 64, "Choose saves for this Steam account; the other copy may be replaced.", 0, white);
          Rect local{dx + 30, y + 540, 190, 42}, cloud{dx + 235, y + 540, 190, 42};
          round(local, 4, 0xffa9721a); add(local, 980); center(local.x, local.y + 10, local.w, "USE LOCAL SAVES", 0, white);
          round(cloud, 4, 0xffa9721a); add(cloud, 981); center(cloud.x, cloud.y + 10, cloud.w, "USE CLOUD SAVES", 0, white);
        }
        if (selected.app_id && selected.app_id == state.cloud_skip_app) {
          text_fit(dx + 32, y + 505, dw - 64, "Cloud saves will not sync for this session.", 0, white);
          Rect play{dx + 30, y + 540, 250, 42};
          round(play, 4, 0xffa9721a); add(play, 982); center(play.x, play.y + 10, play.w, "PLAY WITHOUT CLOUD", 0, white);
        }
        return;
      }
    }
    Rect list{x + 1, y + 106, 292, wr.h - 107};
    rect(list, 0xff392d20);
    text(list.x + 20, list.y + 22, "GAMES", 1, white);
    text(list.x + 20, list.y + 62, "Installed on this PS5", 0, dim);
    Rect quake{list.x + 8, list.y + 99, list.w - 16, 46};
    if (library_selection < 0 || focus == 964 || hover == 964) rect(quake, 0xff64523c);
    add(quake, 964);
    text(quake.x + 12, quake.y + 11, "Quake II RTX", 0, white);
    for (int row = 0; row < 8; ++row) {
      int i = library_page * 8 + row;
      if (i >= game_inventory.count) break;
      const auto &game = game_inventory.entries[i];
      if (game.steam_app == 1089130) continue;
      char label[80];
      if (game.steam_app == 431240) std::snprintf(label, sizeof(label), "Golf With Your Friends");
      else if (game.steam_app) std::snprintf(label, sizeof(label), "Steam game %u", game.steam_app);
      else std::snprintf(label, sizeof(label), "Local installation");
      Rect item{list.x + 8, list.y + 151 + row * 47, list.w - 16, 44};
      if (library_selection == i || focus == 900 + i || hover == 900 + i) rect(item, 0xff64523c);
      add(item, 900 + i);
      text_fit(item.x + 12, item.y + 9, item.w - 24, label, 0, white);
    }
    if (library_page > 0) {
      add({list.x + 12, list.y + 545, 110, 42}, 967);
      text(list.x + 20, list.y + 555, "Previous", 0, white);
    }
    if ((library_page + 1) * 8 < game_inventory.count) {
      add({list.x + 145, list.y + 545, 110, 42}, 968);
      text(list.x + 160, list.y + 555, "Next", 0, white);
    }
    int dx = x + 293, dw = wr.w - 294;
    rect({dx, y + 106, dw, 230}, 0xff604c34);
    if (library_selection < 0 || library_selection >= game_inventory.count) {
      icon(dx + 32, y + 153, 90, 4);
      text(dx + 150, y + 172, "Quake II RTX", 2, white);
      rect({dx, y + 336, dw, 84}, 0xff4b3b26);
      Rect play{dx + 30, y + 351, 150, 54};
      round(play, 4, (hover == 30 || focus == 30) ? 0xff38be84 : 0xff2b9c66);
      add(play, 30);
      center(play.x, play.y + 15, play.w, "PLAY", 1, white);
      text(dx + 205, y + 366, "Installed", 0, dim);
    } else {
      const auto &game = game_inventory.entries[library_selection];
      char title[80];
      if (game.steam_app == 431240) std::snprintf(title, sizeof(title), "Golf With Your Friends");
      else if (game.steam_app) std::snprintf(title, sizeof(title), "Steam game %u", game.steam_app);
      else std::snprintf(title, sizeof(title), "Local installation");
      text_fit(dx + 32, y + 176, dw - 64, title, 1, white);
      rect({dx, y + 336, dw, 84}, 0xff4b3b26);
      text(dx + 32, y + 365, games::state_label(game.state), 0, white);
      text_fit(dx + 32, y + 456, dw - 64, "Launch service is not connected for this game yet.", 0, dim);
    }
    text(dx + 32, y + 540, "Steam account and online library are not connected yet.", 0, dim);
    if (game_inventory.truncated) text(dx + 32, y + 580, "Installed game list is incomplete.", 0, dim);
    return;
  }
  text(x + 230, y + 64, location_name(file_location), 1, ink());
  const int locations[] = {400, 401, 402, 403, 404, 409};
  const char *labels[] = {"Home", "Games", "WinBrew 11 files", "USB drive", "App files", "PS5 data"};
  for (int i = 0; i < 6; ++i) {
    Rect r{x + 16, y + 135 + 47 * i, 176, 40};
    if (locations[i] == file_location) {
      round(r, 5, dark ? 0xff4c413c : 0xfff5ece6);
      round({r.x + 2, r.y + 10, 3, 20}, 1, accent_color());
    }
    button(r, locations[i], labels[i]);
  }
  if (!file_path[0]) {
    text(x + 230, y + 125, "This PC", 2, ink());
    text(x + 230, y + 175, "Choose a location to browse files.", 0, muted());
    const char *labels[] = {"WinBrew 11 files", "USB drive", "App files", "Games"};
    int ids[] = {402, 403, 404, 401};
    for (int i = 0; i < 4; ++i) {
      Rect r{x + 230 + (i % 2) * 420, y + 235 + (i / 2) * 110, 390, 88};
      add(r, ids[i]);
      round(r, 7, dark ? 0xff3b3330 : 0xffffffff);
      if (hover == ids[i] || focus == ids[i])
        round(r, 7, dark ? 0xff4c413c : 0xfff5ece6);
      icon(r.x + 18, r.y + 20, 48, 5);
      text(r.x + 84, r.y + 31, labels[i], 1, ink());
    }
    return;
  }
  button({x + 220, y + 110, 64, 40}, 405, "Up");
  button({x + 292, y + 110, 100, 40}, 406, "Refresh");
  text_fit(x + 410, y + 123, wr.w - 455, file_path, 0, muted());
  if (file_error) {
    const bool denied = file_error == EACCES || file_error == EPERM;
    const bool missing = file_error == ENOENT || file_error == ENODEV;
    const char *title = denied ? "Access to this folder is restricted."
                        : usb_path(file_path) && missing
                            ? "USB drive unavailable."
                        : missing ? "This folder is unavailable."
                                  : "This folder could not be read.";
    text(x + 235, y + 195, title, 1, ink());
    const char *help =
        denied ? "The PS5 has not granted WinBrew 11 access to this location."
        : usb_path(file_path)
            ? "Connect a supported drive, then choose Refresh."
        : missing ? "The location may have moved or is not mounted."
                  : "Choose Refresh to try again, or open another location.";
    text_fit(x + 235, y + 235, wr.w - 275, help, 0, muted());
    char detail[70];
    std::snprintf(detail, sizeof(detail), "%s | Error %d", file_error_stage,
                  file_error);
    text(x + 235, y + 278, detail, 0, muted());
    return;
  }
  if (file_preview_open) {
    text(x + 235, y + 190, "Text preview", 1, ink());
    char line[100];
    int row = 0, col = 0;
    for (const char *p = file_preview; *p && row < 10; ++p) {
      if (*p == '\r')
        continue;
      if (*p == '\n' || col == 80) {
        line[col] = 0;
        text(x + 235, y + 230 + row++ * 30, line, 0, ink());
        col = 0;
        if (*p == '\n')
          continue;
      }
      line[col++] = *p;
    }
    if (col && row < 10) {
      line[col] = 0;
      text(x + 235, y + 230 + row * 30, line, 0, ink());
    }
    button({x + 235, y + 650, 140, 42}, 406, "Back to files");
    return;
  }
  if (!file_count) {
    text(x + 235, y + 195,
         usb_path(file_path) ? "No files are available on this USB location."
                             : "This folder is empty.",
         0, muted());
    if (usb_path(file_path))
      text_fit(x + 235, y + 235, wr.w - 275,
               "If files are missing, reconnect the drive and choose Refresh.",
               0, muted());
  }
  for (int i = 0; i < 10 && file_page * 10 + i < file_count; ++i) {
    auto &e = files[file_page * 10 + i];
    Rect r{x + 220, y + 170 + i * 48, wr.w - 250, 42};
    add(r, 500 + i);
    if (hover == 500 + i || focus == 500 + i || file_selected == file_page * 10 + i)
      round(r, 5, dark ? 0xff4c413c : 0xfff5ece6);
    icon(r.x + 10, r.y + 9, 24, e.directory ? 5 : 1);
    text_fit(r.x + 50, r.y + 12, r.w - 160, e.name, 0, ink());
    text(r.x + r.w - 100, r.y + 12, e.directory ? "Folder" : "File", 0,
         muted());
  }
  char status[90];
  if ((file_transfer.state == fileops::State::failed ||
       file_transfer.state == fileops::State::cancelled) &&
      file_transfer.destination_created &&
      !std::strcmp(file_path, file_transfer.directory)) {
    const char *name = std::strrchr(file_transfer.destination, '/');
    char incomplete[320];
    std::snprintf(incomplete, sizeof(incomplete), "Incomplete copy kept: %.255s",
                   name ? name + 1 : file_transfer.destination);
    text_fit(x + 235, y + wr.h - 91, wr.w - 275, incomplete, 0, muted());
  }
  std::snprintf(status, sizeof(status),
                "%d%s items  |  Page %d  |  Double-click to open", file_count,
                file_truncated ? "+" : "", file_page + 1);
  text(x + 235, y + wr.h - 51, status, 0, muted());
  button({x + wr.w - 220, y + wr.h - 58, 90, 40}, 407, "Previous");
  button({x + wr.w - 120, y + wr.h - 58, 90, 40}, 408, "Next");
}
} // namespace ww
