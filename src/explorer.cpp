// SPDX-License-Identifier: GPL-3.0-or-later
#include "explorer_directory.hpp"
#include "privilege.hpp"
#include "image_codec.hpp"
#include "vapor_game_details.hpp"
#include "shell.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <functional>
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
  if (!std::strcmp(name, ".") || !std::strcmp(name, "..") || (!shell.file_show_hidden && !std::strcmp(name,".winbrew-trash")))
    return true;
  if (std::strlen(name) >= sizeof(shell.files[0].name))
    return true;
  if (shell.file_count == Shell::MaximumFiles) {
    shell.file_truncated = true;
    return false;
  }
  shell.files.emplace_back();
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
  window=1;context=start=false;
  if(id==401){file_games=true;games::refresh(game_inventory);library_selection=-1;library_page=0;library_downloads=false;steam_panel=false;return;}
  file_games=false;
  if(id==405){file_up();return;}if(id==406){file_refresh();file_drive_next_scan=0;file_branches.clear();return;}
  if(id==407||id==408){file_scroll_by(id==407?-10:10);return;}
  if(id==400||id==420||id==421){
    file_location=id;file_path[0]=file_root[0]=file_preview[0]=file_query[0]=0;
    files.clear();file_matches.clear();file_count=file_scroll=file_page=file_compat_page=file_error=file_visible_count=0;file_selected=-1;file_selection.clear();file_preview_open=false;
    file_history_record();return;
  }
  std::string path;
  if(id>=430&&id<=435){static const char*names[]={"Desktop","Downloads","Documents","Pictures","Music","Videos"};path=file_user_root()+"/"+names[id-430];fileops::ensure_folder(path.c_str());}
  else if(id==436){char desktop[1024];desktop_folder(desktop,sizeof(desktop));fileops::ensure_folder(desktop);path=std::string(desktop)+"/.winbrew-trash";fileops::ensure_folder(path.c_str());}
  else if(id==402)path=file_c_root();
  else if(id==424)path="/";
  else if(id==403)path="/mnt/usb0";else if(id==404)path="/app0";else if(id==409)path="/data";
  else if(id==422)path="/mnt/usb1";else if(id==423)path="/mnt/ext0";else return;
  file_open(path.c_str());
}
void Shell::file_open(const char *path) {
  if(!path||!*path||std::strlen(path)>=sizeof(file_path)){file_failure(*this,path&&*path?ENAMETOOLONG:ENOENT,"Open folder");return;}
  char copy[1024];std::snprintf(copy,sizeof(copy),"%s",path);std::snprintf(file_path,sizeof(file_path),"%s",copy);
  while(std::strlen(file_path)>1 && file_path[std::strlen(file_path)-1]=='/')file_path[std::strlen(file_path)-1]=0;
  file_page=file_compat_page=file_scroll=0;file_games=false;file_preview_open=false;file_preview[0]=file_query[0]=0;file_selection.clear();
  if(text_target==TextTarget::file_address||text_target==TextTarget::file_search)end_text();
  const auto user=file_user_root(),croot=file_c_root();std::snprintf(file_root,sizeof(file_root),"%s",croot.c_str());file_location=402;
  auto prefix=[&](const std::string&root){return std::string(file_path)==root || std::string(file_path).find(root+"/")==0;};
  if(prefix("/mnt/usb0")){file_location=403;std::snprintf(file_root,sizeof(file_root),"/mnt/usb0");}
  else if(prefix("/mnt/usb1")){file_location=422;std::snprintf(file_root,sizeof(file_root),"/mnt/usb1");}
  else if(prefix("/mnt/ext0")){file_location=423;std::snprintf(file_root,sizeof(file_root),"/mnt/ext0");}
  else if(prefix(user)){
    std::snprintf(file_root,sizeof(file_root),"%s",croot.c_str());
    static const char*names[]={"Desktop","Downloads","Documents","Pictures","Music","Videos"};for(int i=0;i<6;++i)if(prefix(user+"/"+names[i]))file_location=430+i;
    if(prefix(user+"/Desktop/.winbrew-trash"))file_location=436;
  }else if(prefix(croot)){file_location=402;std::snprintf(file_root,sizeof(file_root),"%s",croot.c_str());}
  else if(prefix("/data")){file_location=409;std::snprintf(file_root,sizeof(file_root),"/data");}
  else if(prefix("/app0")){file_location=404;std::snprintf(file_root,sizeof(file_root),"/app0");}
  else std::snprintf(file_root,sizeof(file_root),"/");
  for(const auto&drive:file_drives)if(!drive.location&&prefix(drive.path)){std::snprintf(file_root,sizeof(file_root),"%s",drive.path.c_str());break;}
  file_refresh();file_history_record();
}
void Shell::file_refresh() {
  std::string selected_name=file_selected>=0&&file_selected<file_count?files[file_selected].name:"";
  files.clear();file_matches.clear();file_preview_for.clear();file_visible_count=0;
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
  file_sort_list();
  for(int i=0;i<file_count;++i)if(selected_name==files[i].name)file_selected=i;
  file_selection.erase(std::remove_if(file_selection.begin(),file_selection.end(),[&](const std::string&name){return std::none_of(files.begin(),files.end(),[&](const FileEntry&e){return name==e.name;});}),file_selection.end());

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
  if(SheetDocument::supports(path))
    sheet_open(path);
  else if (Paint::supports(path))
    paint.open(*this, path);
  else
    notepad_open(path);
}
void Shell::file_up() {
  if(!file_path[0])return;
  if(!std::strcmp(file_path,file_root)){file_location_open(420);return;}
  std::string parent=file_path;auto slash=parent.find_last_of('/');
  if(slash==std::string::npos||slash==0){file_location_open(420);return;}
  parent.resize(slash);file_open(parent.c_str());
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
        else if (state.message[0]) text_fit(list.x + 20, list.y + 106, 160, state.message, 0, kVaporStatusGold);
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
            const auto feedback = vapor_play_feedback(selected.activity, now_ms);
            if (feedback.busy) {
              round({play.x - 5, play.y - 5, play.w + 10, play.h + 10}, 9, feedback.halo);
              round({play.x - 2, play.y - 2, play.w + 4, play.h + 4}, 6, feedback.rim);
            }
            round(play, 4, feedback.fill);
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
            text_fit(dx + 30, y + 348, dw - 60, state.message, 0, kVaporStatusGold);
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
          text_fit(dx + 32, y + 454, dw - 64, state.message[0] ? state.message : "Finish Setup completes PS5 registration. Then reopen Vapor and press Play.", 0, kVaporStatusGold);
        else if (selected.app_id && state.message[0]) text_fit(dx + 32, y + 454, dw - 64, state.message, 0, kVaporStatusGold);
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
  file_browser_ui(wr);
}
} // namespace ww

namespace ww {
void Shell::desktop_folder(char *out,size_t capacity)const {
  int count=std::snprintf(out,capacity,"%s/Desktop",file_user_root().c_str());if(count<0||size_t(count)>=capacity)out[0]=0;
}
void Shell::desktop_files_refresh() {
  desktop_folder(desktop_folder_path,sizeof(desktop_folder_path));
  if(!desktop_folder_path[0]){desktop_file_count=0;return;}
  auto listing=new Shell;std::snprintf(listing->file_path,sizeof(listing->file_path),"%s",desktop_folder_path);listing->file_refresh();
  desktop_file_count=std::min(64,listing->file_count);for(int i=0;i<desktop_file_count;++i)desktop_files[i]=listing->files[i];delete listing;
  desktop_files_next_scan=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())+5000;
}
}
// Functional Windows-style Explorer browser. Vapor keeps its own view above.
namespace ww {
namespace {
uint32_t explorer_rgb(int r,int g,int b){return 0xff000000u|uint32_t(b)<<16|uint32_t(g)<<8|uint32_t(r);}
bool explorer_prefix(const std::string&value,const std::string&root){return value==root || (value.size()>root.size()&&value.compare(0,root.size(),root)==0&&value[root.size()]=='/');}
std::string explorer_leaf(const std::string&path){auto p=path.find_last_of("/\\");return p==std::string::npos?path:path.substr(p+1);}
int explorer_natural(const char*a,const char*b){
 while(*a&&*b){unsigned char ca=*a,cb=*b;if(std::isdigit(ca)&&std::isdigit(cb)){
  const char*aa=a,*bb=b;while(*aa=='0')++aa;while(*bb=='0')++bb;const char*ae=aa,*be=bb;while(std::isdigit((unsigned char)*ae))++ae;while(std::isdigit((unsigned char)*be))++be;
  if(ae-aa!=be-bb)return ae-aa<be-bb?-1:1;int c=std::memcmp(aa,bb,size_t(ae-aa));if(c)return c;while(std::isdigit((unsigned char)*a))++a;while(std::isdigit((unsigned char)*b))++b;continue;
 }ca=std::tolower(ca);cb=std::tolower(cb);if(ca!=cb)return ca<cb?-1:1;++a;++b;}
 return *a?1:*b?-1:0;
}
const char* explorer_type(const FileEntry&e){
 if(e.directory)return "File folder";
 const char*dot=std::strrchr(e.name,'.');if(!dot)return "File";
 if(!std::strcmp(dot,".txt")||!std::strcmp(dot,".log")||!std::strcmp(dot,".md"))return "Text document";
 if(SheetDocument::supports(e.name))return "Spreadsheet";
 if(Paint::supports(e.name))return "Image";
 if(!std::strcmp(dot,".zip"))return "ZIP archive";
 if(!std::strcmp(dot,".exe"))return "Application";
 if(!std::strcmp(dot,".pdf"))return "PDF document";
 return "File";
}
std::string explorer_size(uint64_t bytes){char out[80];if(bytes>=uint64_t(1024)*1024*1024)std::snprintf(out,sizeof(out),"%.2f GB",double(bytes)/(1024.*1024.*1024.));else if(bytes>=1024*1024)std::snprintf(out,sizeof(out),"%.1f MB",double(bytes)/(1024.*1024.));else if(bytes>=1024)std::snprintf(out,sizeof(out),"%.1f KB",double(bytes)/1024.);else std::snprintf(out,sizeof(out),"%llu bytes",(unsigned long long)bytes);return out;}
void explorer_metadata(Shell&s,FileEntry&e){if(e.metadata)return;char path[1300];std::snprintf(path,sizeof(path),"%s/%s",s.file_path,e.name);fileops::inspect(path,e.info);e.metadata=true;}
void explorer_report(Shell&s,const char*message){s.dialog=true;s.focus=60;std::snprintf(s.message,sizeof(s.message),"%s",message);}
}
std::string Shell::file_c_root() const {return console_storage_root[0]?console_storage_root:file_user_root();}
bool Shell::configure_console_storage(const char*console,const char*profile){
 const auto legacy=file_user_root();std::string error;
 if(!console||!profile||std::strlen(console)>=sizeof(console_storage_root)||std::strlen(profile)>=sizeof(user_files_root)||!fileops::prepare_user_profile(legacy.c_str(),console,profile,error)){
  dialog=true;focus=60;std::snprintf(message,sizeof(message),"Personal storage setup could not finish: %s. Your original files are still available.",error.c_str());return false;
 }
 std::snprintf(console_storage_root,sizeof(console_storage_root),"%s",console);std::snprintf(user_files_root,sizeof(user_files_root),"%s",profile);file_drive_next_scan=0;desktop_files_next_scan=0;file_known_folders_ready=false;file_branches.clear();return true;
}
std::string Shell::file_user_root() const {
 if(user_files_root[0])return user_files_root;
 std::string root=preferences;std::replace(root.begin(),root.end(),'\\','/');auto slash=root.find_last_of('/');root=slash==std::string::npos?".":slash==0?"/":root.substr(0,slash);return root;
}
std::string Shell::file_friendly_path(const char*value) const {
 const std::string path=value;if(path=="/")return "Console Root";
 std::vector<std::pair<std::string,std::string>> roots={{file_c_root(),"C:"},{"/mnt/usb0","E:"},{"/mnt/usb1","F:"},{"/mnt/ext0","G:"},{"/app0","R:"}};
 if(!console_storage_root[0])roots.emplace_back("/data","D:");
 for(const auto&drive:file_drives)if(drive.location!=402&&drive.location!=409)roots.emplace_back(drive.path,drive.letter);
 for(const auto&entry:roots)if(explorer_prefix(path,entry.first)){std::string result=entry.second+path.substr(entry.first.size());std::replace(result.begin(),result.end(),'/','\\');return result.size()==2?result+"\\":result;}
 return path;
}
std::string Shell::file_current_label() const {
 if(!file_path[0])return file_location==400?"Home":file_location==421?"Network":"This PC";
 if(explorer_leaf(file_path)==".winbrew-trash")return "Recycle Bin";
 if(std::string(file_path)==file_c_root())return console_storage_root[0]?"Console storage (C:)":"WinBrew (C:)";
 if(user_files_root[0]&&std::string(file_path)==file_user_root())return "WinBrew user";
 if(!std::strcmp(file_path,"/"))return "Console Root";
 for(const auto&drive:file_drives)if(drive.path==file_path)return drive.label;
 return explorer_leaf(file_path);
}
void Shell::file_drive_refresh(){
 auto previous=std::move(file_drives);file_drives.clear();
 const struct Root{const char*label,*path,*letter;int location;} roots[]={
  {console_storage_root[0]?"Console storage (C:)":"WinBrew (C:)",nullptr,"C:",402},{"Console storage (D:)","/data","D:",409},
  {"USB drive (E:)","/mnt/usb0","E:",403},{"USB drive (F:)","/mnt/usb1","F:",422},
  {"Extended storage (G:)","/mnt/ext0","G:",423},{"M.2 storage (H:)","/mnt/ext1","H:",0},
  {"USB drive (I:)","/mnt/usb2","I:",0},{"USB drive (J:)","/mnt/usb3","J:",0},
  {"USB drive (K:)","/mnt/usb4","K:",0},{"USB drive (L:)","/mnt/usb5","L:",0},
  {"USB drive (M:)","/mnt/usb6","M:",0},{"USB drive (N:)","/mnt/usb7","N:",0}};
 for(const auto&entry:roots){if(console_storage_root[0]&&entry.location==409)continue;FileDrive drive;drive.label=entry.label;drive.path=entry.path?entry.path:file_c_root();drive.letter=entry.letter;drive.location=entry.location;
  const bool external=entry.location!=402&&entry.location!=409;
  errno=0;drive.capacity=fileops::volume_space(drive.path.c_str(),drive.total,drive.free,external?drive.path.c_str():nullptr);
  // An empty /mnt/usbN directory belongs to a tiny containing filesystem.
  // Only the exact mounted filesystem qualifies as a removable drive.
  if(external&&!drive.capacity){
   // A backend credential step can briefly own the nonblocking filesystem lock.
   // Keep an already observed device until a real mount query can run again.
   if(errno==EBUSY){auto old=std::find_if(previous.begin(),previous.end(),[&](const FileDrive&item){return item.path==drive.path;});if(old!=previous.end())file_drives.push_back(*old);}
   continue;
  }
  file_drives.push_back(std::move(drive));
 }
  bool changed=previous.size()!=file_drives.size();if(!changed)for(size_t i=0;i<previous.size();++i)if(previous[i].path!=file_drives[i].path){changed=true;break;}
 if(changed){file_branches.clear();if(!files_busy()&&file_path[0]){
  for(const auto&old:previous)if(old.location!=402&&old.location!=409&&explorer_prefix(file_path,old.path)&&std::none_of(file_drives.begin(),file_drives.end(),[&](const FileDrive&drive){return drive.path==old.path;})){
   files.clear();file_matches.clear();file_selection.clear();file_count=file_visible_count=0;file_selected=-1;file_error=ENODEV;file_preview[0]=0;file_preview_for.clear();
  }
  if(file_error==ENODEV)for(const auto&drive:file_drives)if(explorer_prefix(file_path,drive.path)){file_refresh();break;}
 }}
 file_drive_next_scan=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()+2000;
}
void Shell::file_update_matches(){
 file_matches.clear();if(!file_path[0])return;std::string query=file_query;
 std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return std::tolower(c);});
 for(int i=0;i<file_count;++i){std::string name=files[i].name;if(!file_show_hidden&&!name.empty()&&name.front()=='.')continue;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});if(query.empty()||name.find(query)!=std::string::npos)file_matches.push_back(i);}
}
void Shell::file_sort_list(){
 if(file_sort==1||file_sort==3)for(auto&e:files)explorer_metadata(*this,e);
 std::string selected=file_selected>=0&&file_selected<file_count?files[file_selected].name:"";
 std::sort(files.begin(),files.end(),[&](const FileEntry&a,const FileEntry&b){
  if(a.directory!=b.directory)return a.directory>b.directory;
  int result=0;
  if(file_sort==1&&a.info.modified!=b.info.modified)result=a.info.modified<b.info.modified?-1:1;
  else if(file_sort==2)result=std::strcmp(explorer_type(a),explorer_type(b));
  else if(file_sort==3&&a.info.size!=b.info.size)result=a.info.size<b.info.size?-1:1;
  if(!result)result=explorer_natural(a.name,b.name);
  return file_sort_descending?result>0:result<0;
 });
 file_selected=-1;for(int i=0;i<file_count;++i)if(selected==files[i].name)file_selected=i;
 file_sort_dirty=false;file_update_matches();
}
void Shell::file_history_record(){
 if(file_games||file_history_replay)return;
 if(file_tabs.empty()){file_tabs.emplace_back();file_tab=0;}
 auto&tab=file_tabs[file_tab];FilePlace place{file_path,file_location};
 if(tab.position>=0&&tab.history[tab.position].path==place.path&&tab.history[tab.position].location==place.location)return;
 if(tab.position+1<int(tab.history.size()))tab.history.resize(tab.position+1);
 tab.history.push_back(std::move(place));if(tab.history.size()>64)tab.history.erase(tab.history.begin());tab.position=int(tab.history.size())-1;
}
void Shell::file_history_move(int direction){
 if(file_tabs.empty())return;auto&tab=file_tabs[file_tab];int at=tab.position+direction;if(at<0||at>=int(tab.history.size()))return;
 tab.position=at;const auto place=tab.history[at];file_history_replay=true;
 if(place.path.empty())file_location_open(place.location);else file_open(place.path.c_str());file_history_replay=false;
}
void Shell::file_tab_switch(int index){
 if(index<0||index>=int(file_tabs.size())||index==file_tab)return;
 auto&old=file_tabs[file_tab];old.scroll=file_scroll;old.view=file_view;old.query=file_query;
 file_tab=index;const auto place=file_tabs[index].history[file_tabs[index].position];const int scroll=file_tabs[index].scroll,view=file_tabs[index].view;const std::string query=file_tabs[index].query;
 file_history_replay=true;if(place.path.empty())file_location_open(place.location);else file_open(place.path.c_str());file_history_replay=false;
 file_scroll=scroll;file_page=file_compat_page=scroll/10;file_view=view;std::snprintf(file_query,sizeof(file_query),"%s",query.c_str());
}
void Shell::file_tab_new(const char*path){
 if(file_tabs.size()>=8){explorer_report(*this,"Close an Explorer tab before opening another.");return;}
 file_history_record();if(!file_tabs.empty()){auto&old=file_tabs[file_tab];old.scroll=file_scroll;old.view=file_view;old.query=file_query;}
 std::string target=path?path:"";file_tabs.emplace_back();file_tab=int(file_tabs.size())-1;
 if(target.empty())file_location_open(420);else file_open(target.c_str());
}
void Shell::file_tab_close(int index){
 if(index<0||index>=int(file_tabs.size()))return;
 if(file_tabs.size()==1){file_tabs.clear();file_tab=0;window=0;return;}
 const bool active=index==file_tab;file_tabs.erase(file_tabs.begin()+index);if(index<file_tab)--file_tab;
 if(active){file_tab=std::min(index,int(file_tabs.size())-1);const auto place=file_tabs[file_tab].history[file_tabs[file_tab].position];file_history_replay=true;if(place.path.empty())file_location_open(place.location);else file_open(place.path.c_str());file_history_replay=false;}
}
int Shell::file_hit_index(int id) const {int row=id-500;if(row<0||row>=80)return -1;if(file_visible_count&&row<file_visible_count)return file_visible[row];int index=file_page*10+row;return index<file_count?index:-1;}
void Shell::file_select_row(int index,bool toggle){
 if(index<0||index>=file_count)return;
 const std::string name=files[index].name;
 if(toggle){auto at=std::find(file_selection.begin(),file_selection.end(),name);if(at==file_selection.end())file_selection.push_back(name);else file_selection.erase(at);}
 else {file_selection.clear();file_selection.push_back(name);}
 file_selected=index;file_selection_anchor=index;
 if(toggle&&std::find(file_selection.begin(),file_selection.end(),name)==file_selection.end()){
  file_selected=-1;if(!file_selection.empty())for(int i=0;i<file_count;++i)if(file_selection.front()==files[i].name){file_selected=i;break;}
 }
}
void Shell::file_scroll_by(int rows){
 if(window!=1||file_games||dialog||notepad_modal()||file_context||file_properties||file_rename_prompt||files_busy())return;
 int visible=std::max(1,file_visible_count),maximum=std::max(0,int(file_matches.size())-visible);
 file_scroll=std::max(0,std::min(maximum,file_scroll+rows*file_columns));file_page=file_compat_page=file_scroll/10;
}
void Shell::file_nav_scroll_by(int rows){if(window!=1||file_games||dialog||file_context||file_properties||files_busy())return;int visible=file_nav_bounds.h/30;file_nav_scroll=std::max(0,std::min(std::max(0,file_nav_total-visible),file_nav_scroll+rows));}
void Shell::file_position_scroll(int y,bool navigation){
 Rect track=navigation?file_nav_track:file_scroll_track;int count=navigation?file_nav_total:int(file_matches.size());int visible=navigation?file_nav_bounds.h/30:std::max(1,file_visible_count);if(track.h<=0||count<=visible)return;
 int thumb=std::max(28,track.h*visible/count),travel=track.h-thumb;int pos=std::max(0,std::min(travel,y-track.y-thumb/2));int value=travel?int(int64_t(pos)*(count-visible)/travel):0;
 if(navigation)file_nav_scroll=value;else {file_scroll=value;file_page=file_compat_page=value/10;}
}
void Shell::file_context_selected(){
 file_context_desktop=false;shortcut_context_id=0;file_menu_kind=0;file_context_item=file_selected>=0&&file_selected<file_count;
 std::snprintf(file_context_path,sizeof(file_context_path),"%s",file_path);std::snprintf(file_context_folder,sizeof(file_context_folder),"%s",file_path);
 if(file_context_item){std::string selected_path=std::string(file_path)+"/"+files[file_selected].name;if(selected_path.size()>=sizeof(file_context_path)){explorer_report(*this,"This path is too long.");file_context=false;return;}std::snprintf(file_context_path,sizeof(file_context_path),"%s",selected_path.c_str());fileops::inspect(file_context_path,file_context_info);if(files[file_selected].directory)std::snprintf(file_context_folder,sizeof(file_context_folder),"%s",file_context_path);}
 file_context=true;
}
void Shell::file_copy_selection(bool moving){
 std::vector<fileops::Clipboard> next;
 if(file_selection.empty()&&file_selected>=0)file_selection.push_back(files[file_selected].name);
 for(const auto&name:file_selection){fileops::Clipboard item;std::string path=std::string(file_path)+"/"+name;if(!item.copy(path.c_str())){explorer_report(*this,"One of the selected items cannot be copied. Refresh the folder and try again.");return;}item.move=moving;next.push_back(item);}
 if(next.empty())return;file_clipboard=next.front();file_batch_clipboard=std::move(next);
}
void Shell::file_recycle_selection(){
 std::vector<fileops::Clipboard> next;
 if(file_selection.empty()&&file_selected>=0)file_selection.push_back(files[file_selected].name);
 for(const auto&name:file_selection){fileops::Clipboard item;std::string path=std::string(file_path)+"/"+name;if(!item.copy(path.c_str())){explorer_report(*this,"One of the selected items changed or cannot be recycled. Refresh and try again.");return;}next.push_back(item);}
 if(next.empty())return;file_recycle_queue=std::move(next);file_recycle_index=0;file_recycle_active=true;file_context=false;focus=807;
}
void Shell::file_drag_begin(){
 file_drag_items.clear();for(const auto&name:file_selection){fileops::Clipboard item;std::string path=std::string(file_path)+"/"+name;if(!item.copy(path.c_str())){file_drag_pending=false;return;}file_drag_items.push_back(item);}
 file_drag_active=!file_drag_items.empty();file_drag_pending=false;file_drag_collapse=false;end_text();
}
void Shell::file_drop(){
 std::string destination;
 for(int i=count-1;i>=0;--i)if(hits[i].r.has(mx,my)){
  int id=hits[i].id;
  if(id>=500&&id<580){int index=file_hit_index(id);if(index>=0&&files[index].directory)destination=std::string(file_path)+"/"+files[index].name;}
  else if(id>=2400&&id<2656){int index=id-2400;if(index<int(file_nav.size()))destination=file_nav[index].path;}
  else if(id>=3000&&id<3016&&id-3000<int(file_drives.size()))destination=file_drives[id-3000].path;
  else if(id>=2200&&id<2208&&id-2200<int(file_tabs.size())){const auto&tab=file_tabs[id-2200];destination=tab.history[tab.position].path;}
  else if(file_content_bounds.has(mx,my))destination=file_path;
  break;
 }
 file_drag_active=file_drag_pending=false;
 if(destination.empty()){file_drag_items.clear();return;}
 fileops::Info folder{};if(fileops::inspect(destination.c_str(),folder)<0||!folder.directory){file_drag_items.clear();return;}
 bool copying=(file_modifiers&0x11)!=0;
 if(!(file_modifiers&0x22))for(const auto&item:file_drag_items)if(item.info.device!=folder.device)copying=true;
 for(const auto&item:file_drag_items){std::string path=item.path;auto slash=path.find_last_of('/');if((item.info.directory&&explorer_prefix(destination,path)) || (!copying&&slash!=std::string::npos&&path.substr(0,slash)==destination)){file_drag_items.clear();return;}}
 for(auto&item:file_drag_items)item.move=!copying;
 file_batch_queue=std::move(file_drag_items);file_batch_index=0;file_batch_active=!file_batch_queue.empty();file_batch_destination=destination;file_transfer_from_clipboard=false;
 if(file_batch_active){file_transfer.begin(file_batch_queue.front(),destination.c_str());file_transfer_seen=fileops::State::idle;focus=807;}
}
void Shell::file_begin_rename(){
 if(file_selected<0||file_selected>=file_count||file_selection.size()>1)return;
 std::string selected_path=std::string(file_path)+"/"+files[file_selected].name;
 if(selected_path.size()>=sizeof(file_rename_path)){explorer_report(*this,"This path is too long to rename.");return;}
 std::snprintf(file_rename_path,sizeof(file_rename_path),"%s",selected_path.c_str());
 if(fileops::inspect(file_rename_path,file_rename_expected)<0){explorer_report(*this,"This item is unavailable. Refresh the folder and try again.");return;}
 std::snprintf(file_rename_name,sizeof(file_rename_name),"%s",files[file_selected].name);file_rename_prompt=true;file_context=false;file_menu_kind=0;
 begin_text(TextTarget::file_rename);text_selection=true;focus=2178;
}
void Shell::file_rename_submit(){
 int error=0;if(!fileops::rename_item(file_rename_path,file_rename_expected,file_rename_name,error)){
  explorer_report(*this,error==EEXIST?"An item with that name already exists. Enter a different name.":"The item changed, or that name is unavailable. Check the name or refresh the folder.");return;
 }
 const std::string name=file_rename_name;file_rename_prompt=false;end_text();file_refresh();desktop_files_next_scan=0;file_branches.clear();
 for(int i=0;i<file_count;++i)if(name==files[i].name){file_select_row(i);break;}
}
void Shell::file_address_submit(){
 std::string path=file_address;
 if(path=="Console Root"){end_text();file_location_open(424);return;}
 if(path=="Home"){end_text();file_location_open(400);return;}if(path=="This PC"||path=="This PC\\"){end_text();file_location_open(420);return;}if(path=="Network"){end_text();file_location_open(421);return;}
 std::replace(path.begin(),path.end(),'\\','/');
 if(path.size()>=2&&path[1]==':'){
  const char drive=std::toupper((unsigned char)path[0]);std::string root=drive=='C'?file_c_root():drive=='D'?(console_storage_root[0]?file_c_root():"/data"):drive=='E'?"/mnt/usb0":drive=='F'?"/mnt/usb1":drive=='G'?"/mnt/ext0":drive=='R'?"/app0":"";
  if(root.empty())for(const auto&entry:file_drives)if(!entry.letter.empty()&&entry.letter[0]==drive){root=entry.path;break;}
  if(root.empty()){explorer_report(*this,"That drive is not available in WinBrew.");return;}path=root+path.substr(2);
 }
 if(path.empty())return;
 std::vector<std::string> parts;bool absolute=path.front()=='/';size_t at=0;
 while(at<path.size()){auto end=path.find('/',at);if(end==std::string::npos)end=path.size();auto part=path.substr(at,end-at);if(part==".."){if(!parts.empty())parts.pop_back();}else if(!part.empty()&&part!=".")parts.push_back(part);at=end+1;}
 std::string clean=absolute?"/":"";for(const auto&part:parts){if(!clean.empty()&&clean.back()!='/')clean+="/";clean+=part;}
 end_text();file_open(clean.c_str());
}
void Shell::file_popup(int kind,Rect anchor){
 if(text_target==TextTarget::file_address||text_target==TextTarget::file_search)end_text();
 file_menu_kind=kind;file_context=true;file_context_item=false;file_new=false;shortcut_context_id=0;file_context_desktop=false;
 std::snprintf(file_context_folder,sizeof(file_context_folder),"%s",file_path);
 const int rows=kind==1?6:kind==2?3:kind==3?5:5;
 file_context_bounds={std::max(0,std::min(anchor.x,W-240)),std::max(0,std::min(anchor.y+anchor.h,H-54-rows*32-8)),240,rows*32+8};focus=kind==1?2110:kind==2?2120:kind==3?1420:2130;
}
void Shell::file_browser_action(int id){
 if(id>=3000&&id<3016){int i=id-3000;if(i<int(file_drives.size())){const auto drive=file_drives[i];if(drive.location)file_location_open(drive.location);else file_open(drive.path.c_str());}return;}
 if(id>=2200&&id<2208){file_tab_switch(id-2200);return;}if(id==2210){file_tab_new();return;}if(id>=2220&&id<2228){file_tab_close(id-2220);return;}
 if(id>=2300&&id<2332){int i=id-2300;if(i<int(file_crumbs.size())){std::string path=file_crumbs[i];if(path.empty())file_location_open(420);else file_open(path.c_str());}return;}
 if(id>=2400&&id<2656){int i=id-2400;if(i<int(file_nav.size())){const auto nav=file_nav[i];if(nav.location)file_location_open(nav.location);else if(!nav.path.empty())file_open(nav.path.c_str());}return;}
 if(id>=2700&&id<2956){int i=id-2700;if(i<int(file_nav.size())){const auto nav=file_nav[i];if(nav.location==420)file_pc_expanded=!file_pc_expanded;else if(nav.location==421)file_network_expanded=!file_network_expanded;else if(!nav.path.empty()){
  auto at=std::find(file_expanded.begin(),file_expanded.end(),nav.path);if(at!=file_expanded.end())file_expanded.erase(at);else {file_expanded.push_back(nav.path);FileBranch branch;branch.path=nav.path;fileops::list_folders(nav.path.c_str(),branch.children);file_branches.push_back(std::move(branch));}
 }}return;}
 if(id==2100){file_history_move(-1);return;}if(id==2101){file_history_move(1);return;}
 if(id==2102){std::snprintf(file_address,sizeof(file_address),"%s",file_path[0]?file_friendly_path(file_path).c_str():file_current_label().c_str());begin_text(TextTarget::file_address);text_selection=true;focus=2102;return;}
 if(id==2103){begin_text(TextTarget::file_search);text_selection=true;focus=2103;return;}
 if(id==2104){file_up();return;}if(id==2105){file_refresh();file_drive_next_scan=0;file_branches.clear();return;}
 if(id==2106||id==2107||id==2108||id==2109){Rect anchor{mx,my,0,8};for(int i=0;i<count;++i)if(hits[i].id==id){anchor=hits[i].r;break;}file_popup(id==2106?3:id==2107?1:id==2108?2:4,anchor);return;}
 if(id>=2110&&id<=2115){int menu=file_menu_kind;file_context=false;file_menu_kind=0;if(id<=2113){bool same=file_sort==id-2110;file_sort=id-2110;file_sort_descending=same&&menu!=1?!file_sort_descending:file_sort==1;}else file_sort_descending=id==2115;file_sort_list();return;}
 if(id>=2120&&id<=2122){file_view=id-2120;file_scroll=file_page=file_compat_page=0;file_context=false;file_menu_kind=0;return;}
 if(id==2125){file_query[0]=0;file_scroll=file_page=file_compat_page=0;file_selection.clear();file_selected=-1;return;}
 if(id==2127){file_preview_pane=!file_preview_pane;return;}
 if(id==2130||id==2124){file_selection.clear();for(int i:file_matches)file_selection.push_back(files[i].name);if(!file_matches.empty())file_selected=file_matches.front();file_context=false;return;}
 if(id==2131){file_select_mode=!file_select_mode;file_context=false;return;}
 if(id==2132){file_show_hidden=!file_show_hidden;file_context=false;file_refresh();return;}
 if(id==2133){clipboard_text=file_friendly_path(file_path);file_context=false;return;}
 if(id==2134){file_context_selected();file_action(804);return;}
 if(id==2140||id==2141){file_copy_selection(id==2141);return;}
 if(id==2142){file_context_selected();std::snprintf(file_context_folder,sizeof(file_context_folder),"%s",file_path);file_action(803);return;}
 if(id==2143){file_begin_rename();return;}if(id==2144){file_recycle_selection();return;}
 if(id==2145){file_context_selected();file_action(804);return;}
 if(id==2146&&file_selected>=0&&file_selected<file_count&&files[file_selected].directory){std::string path=std::string(file_path)+"/"+files[file_selected].name;file_context=false;file_tab_new(path.c_str());return;}
 if(id==2147){clipboard_text=file_friendly_path(file_context_path);file_context=false;return;}
 if(id==2150||id==2151){std::string path=file_context_path;int location=file_context_nav_location;file_context=false;file_menu_kind=0;if(id==2151){file_tab_new(path.empty()?nullptr:path.c_str());if(path.empty()&&location)file_location_open(location);}else if(path.empty())file_location_open(location);else file_open(path.c_str());return;}
 if(id==2152){file_context=false;file_menu_kind=0;file_properties_multiple=false;if(fileops::inspect(file_context_path,file_properties_info)<0){explorer_report(*this,"Properties are unavailable for this location.");return;}std::snprintf(file_properties_path,sizeof(file_properties_path),"%s",file_context_path);file_properties=true;focus=806;return;}
 if(id==2176){file_rename_submit();return;}if(id==2177){file_rename_prompt=false;end_text();return;}if(id==2178){begin_text(TextTarget::file_rename);text_selection=true;focus=2178;return;}
}
bool Shell::file_browser_key(uint16_t usage,uint8_t modifiers){
 if(window!=1||file_games||dialog||notepad_modal()||file_context||file_properties||files_busy())return false;
 const bool ctrl=(modifiers&0x11)!=0,alt=(modifiers&0x44)!=0;
 if(file_rename_prompt)return false;
 if(alt&&(usage==0x50||usage==0x4f)){file_history_move(usage==0x50?-1:1);return true;}
 if(alt&&usage==0x52){file_up();return true;}
 if(ctrl&&usage==0x0f){file_browser_action(2102);return true;}
 if(ctrl&&usage==0x09){file_browser_action(2103);return true;}
 if(ctrl&&usage==0x17){file_tab_new();return true;}
 if(ctrl&&usage==0x1a){file_tab_close(file_tab);return true;}
 if(text_target!=TextTarget::none)return false;
 if(ctrl&&usage==4){file_browser_action(2130);return true;}
 if(ctrl&&(usage==6||usage==0x1b)){file_copy_selection(usage==0x1b);return true;}
 if(ctrl&&usage==0x19){file_browser_action(2142);return true;}
 if(usage==0x3b){file_begin_rename();return true;}if(usage==0x3e){file_refresh();return true;}
 if(usage==0x4c){file_recycle_selection();return true;}
 if(usage==0x2a){if(!file_tabs.empty()&&file_tabs[file_tab].position>0)file_history_move(-1);else file_up();return true;}
 if(usage==0x4b||usage==0x4e){file_scroll_by(usage==0x4b?-std::max(1,file_visible_count/file_columns):std::max(1,file_visible_count/file_columns));return true;}
 if((usage==0x52||usage==0x51||usage==0x4a||usage==0x4d)&&!file_matches.empty()&&file_selected>=0){
  auto at=std::find(file_matches.begin(),file_matches.end(),file_selected);int index=at==file_matches.end()?0:int(at-file_matches.begin());
  index=usage==0x4a?0:usage==0x4d?int(file_matches.size())-1:std::max(0,std::min(int(file_matches.size())-1,index+(usage==0x52?-file_columns:file_columns)));
  file_select_row(file_matches[index]);if(index<file_scroll)file_scroll=index;else if(index>=file_scroll+std::max(1,file_visible_count))file_scroll=index-std::max(1,file_visible_count)+1;
  file_page=file_compat_page=file_scroll/10;focus=500+std::max(0,index-file_scroll);return true;
 }
 return false;
}
} // namespace ww
namespace ww {
void Shell::file_browser_ui(Rect wr){
 const uint32_t page=dark?explorer_rgb(25,25,25):0xffffffff,chrome=dark?explorer_rgb(32,32,32):explorer_rgb(244,248,251),sidebar=dark?explorer_rgb(31,31,31):explorer_rgb(248,250,252),edge=dark?explorer_rgb(59,59,59):explorer_rgb(218,224,229),selected=dark?explorer_rgb(49,76,96):explorer_rgb(204,232,255),over=dark?explorer_rgb(43,48,53):explorer_rgb(229,243,255),blue=explorer_rgb(0,103,192);
 const int x=wr.x,y=wr.y,w=wr.w,h=wr.h;
 if(preferences[0]&&!file_known_folders_ready){const auto root=file_user_root();for(const char*name:{"Desktop","Downloads","Documents","Pictures","Music","Videos"})fileops::ensure_folder((root+"/"+name).c_str());file_known_folders_ready=true;}
 if(file_location==401)file_location=420;
 file_history_record();
 const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());if(now>=file_drive_next_scan)file_drive_refresh();
 rect({x+1,y+1,w-152,41},chrome);rect({x+1,y+43,w-2,h-44},page);
 // Tabs sit in the title area; standard window controls keep their existing hits.
 const int tabwidth=std::max(110,std::min(220,(w-212)/std::max(1,int(file_tabs.size()))));
 for(int i=0;i<int(file_tabs.size());++i){Rect tab{x+8+i*tabwidth,y+5,tabwidth-3,37};bool active=i==file_tab;round(tab,7,active?page:chrome);add(tab,2200+i);
  const auto&place=file_tabs[i].history[file_tabs[i].position];std::string label=active?file_current_label():place.path.empty()?(place.location==400?"Home":place.location==421?"Network":"This PC"):explorer_leaf(place.path);
  icon(tab.x+12,tab.y+10,17,place.path.empty()?1:5);small_fit(tab.x+38,tab.y+10,tab.w-72,label.c_str(),ink());
  Rect close{tab.x+tab.w-28,tab.y+5,24,27};add(close,2220+i);line(close.x+8,close.y+8,close.x+15,close.y+15,muted());line(close.x+15,close.y+8,close.x+8,close.y+15,muted());
 }
 if(file_tabs.size()<8){Rect plus{x+12+int(file_tabs.size())*tabwidth,y+7,30,30};add(plus,2210);line(plus.x+10,plus.y+15,plus.x+20,plus.y+15,ink());line(plus.x+15,plus.y+10,plus.x+15,plus.y+20,ink());}
 // Address/search row.
 rect({x+1,y+43,w-2,51},chrome);
 auto arrow=[&](Rect r,int id,int direction,bool enabled){if(enabled)add(r,id);if(enabled&&(hover==id||focus==id))round(r,4,over);uint32_t c=enabled?ink():muted();int cx=r.x+r.w/2,cy=r.y+r.h/2;if(direction==2){line(cx-5,cy+2,cx,cy-3,c);line(cx,cy-3,cx+5,cy+2,c);line(cx,cy-3,cx,cy+6,c);}else{int sign=direction==0?-1:1;line(cx-sign*6,cy,cx+sign*5,cy,c);line(cx+sign*5,cy,cx,cy-5,c);line(cx+sign*5,cy,cx,cy+5,c);}};
 const auto&tab=file_tabs[file_tab];arrow({x+10,y+50,34,34},2100,0,tab.position>0);arrow({x+50,y+50,34,34},2101,1,tab.position+1<int(tab.history.size()));arrow({x+94,y+50,34,34},2104,2,file_path[0]);
 Rect refresh{x+133,y+50,32,34};add(refresh,2105);small_text(refresh.x+9,refresh.y+8,"O",ink());line(refresh.x+22,refresh.y+9,refresh.x+22,refresh.y+15,ink());line(refresh.x+17,refresh.y+15,refresh.x+22,refresh.y+15,ink());
 const int searchw=250;file_address_bounds={x+175,y+49,w-175-searchw-24,36};file_search_bounds={x+w-searchw-11,y+49,searchw,36};
 round(file_address_bounds,5,page);add(file_address_bounds,2102);icon(file_address_bounds.x+12,file_address_bounds.y+9,18,1);
 file_crumbs.clear();
 if(text_target==TextTarget::file_address)draw_text_field(file_address_bounds.x+39,file_address_bounds.y+6,file_address_bounds.w-48,TextTarget::file_address,"Enter a folder path");
 else{
  int cx=file_address_bounds.x+40,limit=file_address_bounds.x+file_address_bounds.w-10;
  auto crumb=[&](const std::string&label,const std::string&path){if(cx>=limit-20||file_crumbs.size()>=32)return;int index=int(file_crumbs.size());file_crumbs.push_back(path);int width=std::min(small_width(label.c_str())+18,limit-cx);Rect r{cx,file_address_bounds.y+2,width,31};add(r,2300+index);if(hover==2300+index||focus==2300+index)round(r,3,over);small_fit(cx+4,r.y+7,width-9,label.c_str(),ink());cx+=width;if(cx<limit-8){small_text(cx,r.y+6,">",muted());cx+=14;}};
  if(!file_path[0])crumb(file_current_label(),"");else{
   crumb("This PC","");const std::string friendly=file_friendly_path(file_path);std::string actual=file_path,root=file_root;std::string label=file_friendly_path(root.c_str());if(label.size()>2&&label[1]==':')label.resize(2);
   if(explorer_prefix(actual,root)){crumb(label,root);std::string tail=actual.substr(root.size());size_t at=0;std::string accum=root;while(at<tail.size()){auto end=tail.find('/',at);if(end==std::string::npos)end=tail.size();auto part=tail.substr(at,end-at);if(!part.empty()){accum+="/"+part;crumb(part==".winbrew-trash"?"Recycle Bin":part,accum);}at=end+1;}}
   else crumb(explorer_leaf(actual),actual);
  }
 }
 round(file_search_bounds,5,page);add(file_search_bounds,2103);icon(file_search_bounds.x+file_search_bounds.w-29,file_search_bounds.y+10,17,7);
 if(text_target==TextTarget::file_search)draw_text_field(file_search_bounds.x+12,file_search_bounds.y+6,file_search_bounds.w-48,TextTarget::file_search,"Search this folder");else small_fit(file_search_bounds.x+12,file_search_bounds.y+10,file_search_bounds.w-44,file_query[0]?file_query:(file_path[0]?("Search "+file_current_label()).c_str():file_location==400?"Find a folder or drive":"Find a drive"),file_query[0]?ink():muted());
 if(file_query[0]){Rect clear{file_search_bounds.x+file_search_bounds.w-53,file_search_bounds.y+4,22,27};add(clear,2125);small_text(clear.x+7,clear.y+4,"x",muted());}
 // Windows command bar: icon commands and compact labelled dropdowns.
 rect({x+1,y+94,w-2,49},page);line(x+1,y+143,x+w-2,y+143,edge);
 auto command=[&](int dx,int width,int id,const char*label,bool enabled,int glyph,bool drop=false){Rect r{x+dx,y+100,width,34};if(enabled)add(r,id);if(enabled&&(hover==id||focus==id))round(r,4,over);uint32_t c=enabled?ink():muted();if(glyph>=0){int gx=r.x+11,gy=r.y+9;if(glyph==0){line(gx,gy+8,gx+14,gy+8,c);line(gx+7,gy+1,gx+7,gy+15,c);}else if(glyph==1){line(gx,gy,gx+14,gy+14,c);line(gx+14,gy,gx,gy+14,c);}else if(glyph==2){rect({gx+4,gy+3,12,14},edge);rect({gx,gy,12,14},c);rect({gx+1,gy+1,10,12},page);}else if(glyph==3){rect({gx+2,gy+2,13,14},c);rect({gx+3,gy+3,11,12},page);rect({gx+5,gy,7,4},c);}else if(glyph==4){small_text(gx,gy-1,"A",c);}else if(glyph==5){line(gx+2,gy+3,gx+14,gy+3,c);rect({gx+4,gy+5,9,11},c);rect({gx+5,gy+6,7,9},page);}else if(glyph==6){line(gx,gy+2,gx+13,gy+2,c);line(gx,gy+7,gx+9,gy+7,c);line(gx,gy+12,gx+5,gy+12,c);}else if(glyph==7){for(int j=0;j<3;++j){rect({gx,gy+1+j*5,3,3},c);line(gx+6,gy+2+j*5,gx+14,gy+2+j*5,c);}}}
  if(label&&*label)small_text(r.x+(glyph>=0?34:12),r.y+9,label,c);if(drop){int ax=r.x+r.w-13,ay=r.y+15;line(ax-3,ay,ax,ay+3,c);line(ax,ay+3,ax+3,ay,c);}};
 const bool chosen=file_selected>=0&&file_selected<file_count;
 command(10,93,2106,"New",file_path[0],0,true);line(x+112,y+106,x+112,y+130,edge);
 command(120,38,2141,"",chosen,1);command(163,38,2140,"",chosen,2);command(206,38,2142,"",file_path[0]&&file_clipboard.ready,3);command(249,38,2143,"",chosen&&file_selection.size()<=1,4);command(292,38,2144,"",chosen,5);line(x+344,y+106,x+344,y+130,edge);
 command(357,93,2107,"Sort",file_path[0],6,true);command(457,102,2108,"View",file_path[0],7,true);command(568,42,2109,"...",file_path[0],-1);
 command(w-114,105,2127,"Preview",file_path[0],-1);
 // Navigation hierarchy, left-aligned throughout.
 file_nav_bounds={x+1,y+144,240,h-177};rect(file_nav_bounds,sidebar);line(x+241,y+144,x+241,y+h-34,edge);
 file_nav.clear();auto nav=[&](std::string label,int location,std::string path,int kind=5,int depth=0,bool expandable=false,bool expanded=false,bool group=false){if(file_nav.size()<256)file_nav.push_back({std::move(label),std::move(path),location,depth,kind,group,expandable,expanded});};
 nav("Home",400,"",1);nav("Quick access",0,"",0,0,false,false,true);
 const char*folders[]={"Desktop","Downloads","Documents","Pictures","Music","Videos"};for(int i=0;i<6;++i)nav(folders[i],430+i,file_user_root()+"/"+folders[i],i==0?1:5);
 nav("Recycle Bin",436,file_user_root()+"/Desktop/.winbrew-trash",2);
 nav("This PC",420,"",1,0,true,file_pc_expanded);
 std::function<void(const std::string&,int)> children_nav=[&](const std::string&path,int depth){if(depth>8||file_nav.size()>240)return;auto branch=std::find_if(file_branches.begin(),file_branches.end(),[&](const FileBranch&b){return b.path==path;});if(branch==file_branches.end()){FileBranch entry;entry.path=path;fileops::list_folders(path.c_str(),entry.children);file_branches.push_back(std::move(entry));branch=file_branches.end()-1;}const auto names=branch->children;for(const auto&name:names){std::string child=path+"/"+name;bool expanded=std::find(file_expanded.begin(),file_expanded.end(),child)!=file_expanded.end();nav(name,0,child,5,depth,true,expanded);if(expanded)children_nav(child,depth+1);}};
 if(file_pc_expanded)for(const auto&drive:file_drives){bool expanded=std::find(file_expanded.begin(),file_expanded.end(),drive.path)!=file_expanded.end();nav(drive.label,drive.location,drive.path,1,1,true,expanded);if(expanded)children_nav(drive.path,2);}
 nav("Network",421,"",1,0,true,file_network_expanded);
 nav("WinBrew",0,"",0,0,false,false,true);nav("Vapor library",401,"",16);nav("Console Root",424,"/",1);nav("App files (read-only)",404,"/app0",5);
 file_nav_total=int(file_nav.size());int navvisible=file_nav_bounds.h/30;file_nav_scroll=std::max(0,std::min(std::max(0,file_nav_total-navvisible),file_nav_scroll));
 for(int row=0;row<navvisible&&row+file_nav_scroll<file_nav_total;++row){int index=row+file_nav_scroll;const auto&entry=file_nav[index];Rect r{x+4,y+150+row*30,232,29};
  if(entry.group){line(r.x+11,r.y+3,r.x+r.w-10,r.y+3,edge);small_text(r.x+16,r.y+10,entry.label.c_str(),muted(),6);continue;}
  bool active=entry.path.empty()?(!file_path[0]&&entry.location==file_location):entry.path==file_path;int id=2400+index;add(r,id);if(active||hover==id||focus==id)rect(r,active?selected:over);
  int indent=entry.depth*13;if(entry.expandable){Rect disclosure{r.x+5+indent,r.y+1,20,27};add(disclosure,2700+index);int cx=disclosure.x+10,cy=disclosure.y+13;if(entry.expanded){line(cx-3,cy-2,cx,cy+1,muted());line(cx,cy+1,cx+3,cy-2,muted());}else{line(cx-2,cy-3,cx+1,cy,muted());line(cx+1,cy,cx-2,cy+3,muted());}}
  icon(r.x+27+indent,r.y+5,18,entry.kind);small_fit(r.x+53+indent,r.y+7,r.w-61-indent,entry.label.c_str(),ink());
 }
 file_nav_track={x+232,y+151,7,h-190};if(file_nav_total>navvisible){int thumb=std::max(28,file_nav_track.h*navvisible/file_nav_total);int top=file_nav_track.y+(file_nav_track.h-thumb)*file_nav_scroll/std::max(1,file_nav_total-navvisible);round({file_nav_track.x,top,5,thumb},2,muted());}
 // Folder/drive area.
 const int left=x+260,areaW=w-281;file_content_bounds={left,y+182,areaW,h-218};file_window_bounds=wr;
 auto section=[&](int top,const char*label){line(left+7,top+7,left+12,top+12,muted());line(left+12,top+12,left+17,top+7,muted());small_text(left+23,top,label,blue,8);int start=left+30+small_width(label,8);line(start,top+10,x+w-22,top+10,edge);};
 auto drive_card=[&](Rect r,const FileDrive&drive,int id){add(r,id);if(hover==id||focus==id)rect(r,over);int gx=r.x+7,gy=r.y+17;rect({gx,gy+10,39,23},explorer_rgb(109,117,122));rect({gx+1,gy+1,37,16},explorer_rgb(222,226,230));line(gx,gy+10,gx+9,gy,explorer_rgb(222,226,230),3);line(gx+30,gy,gx+38,gy+10,explorer_rgb(222,226,230),3);rect({gx+5,gy+25,5,4},explorer_rgb(66,184,94));
  int tx=r.x+57;small_fit(tx,r.y+8,r.w-66,drive.label.c_str(),ink());if(drive.capacity){int bw=r.w-68;rect({tx,r.y+30,bw,12},explorer_rgb(205,208,212));rect({tx+1,r.y+31,bw-2,10},explorer_rgb(235,236,238));uint64_t used=drive.total-drive.free;int fill=drive.total?int(used*uint64_t(bw-2)/drive.total):0;rect({tx+1,r.y+31,fill,10},used>drive.total*9/10?explorer_rgb(199,38,42):explorer_rgb(0,120,212));std::string caption=explorer_size(drive.free)+" free of "+explorer_size(drive.total);small_fit(tx,r.y+48,r.w-66,caption.c_str(),muted(),6);}else small_fit(tx,r.y+38,r.w-66,"Space information unavailable",muted(),6);
 };
 file_visible_count=0;file_scroll_track={};
 if(!file_path[0]){
  std::string location_query=file_query;std::transform(location_query.begin(),location_query.end(),location_query.begin(),[](unsigned char c){return std::tolower(c);});
  auto matches_location=[&](std::string label){std::transform(label.begin(),label.end(),label.begin(),[](unsigned char c){return std::tolower(c);});return location_query.empty()||label.find(location_query)!=std::string::npos;};
  if(file_location==421){section(y+160,"Network locations");small_text(left+24,y+210,"No network locations are mounted.",muted());small_fit(left+24,y+243,areaW-48,"Connected storage appears under This PC. Network shares must be mounted before they can be browsed.",muted(),6);}
  else{
   int top=y+160;
   if(file_location==400){section(top,"Quick access");int cols=std::max(1,areaW/280);for(int i=0,position=0;i<6;++i){if(!matches_location(folders[i]))continue;int slot=position++;Rect r{left+12+(slot%cols)*(areaW/cols),top+34+(slot/cols)*67,areaW/cols-12,60};add(r,430+i);if(hover==430+i||focus==430+i)rect(r,over);icon(r.x+10,r.y+12,34,5);small_fit(r.x+57,r.y+20,r.w-65,folders[i],ink());}top+=38+((6+cols-1)/cols)*67;}
   section(top,"Devices and drives");const int cols=std::max(1,std::min(3,areaW/275));int width=areaW/cols;
   for(size_t i=0,position=0;i<file_drives.size();++i){if(!matches_location(file_drives[i].label))continue;int slot=int(position++);drive_card({left+8+(slot%cols)*width,top+32+(slot/cols)*80,width-12,75},file_drives[i],3000+int(i));}
   int networktop=top+49+int((file_drives.size()+cols-1)/cols)*80;section(networktop,"Network locations");small_text(left+25,networktop+35,"No network locations are mounted.",muted(),6);
  }
 }else{
  if(file_sort_dirty)file_sort_list();file_update_matches();
  if(file_page!=file_compat_page){file_scroll=file_page*10;file_compat_page=file_page;}
  const int preview=file_preview_pane?270:0,listW=areaW-preview;
  const int rowheight=file_view==2?72:file_view==1?29:32;file_columns=file_view==2?std::max(1,listW/300):1;
  const int top=y+(file_view==0?189:157),bottom=y+h-38,rows=std::max(1,(bottom-top)/rowheight),slots=std::min(80,rows*file_columns);
  file_scroll=std::max(0,std::min(std::max(0,int(file_matches.size())-slots),file_scroll));file_page=file_compat_page=file_scroll/10;
  file_content_bounds={left,top,listW-8,bottom-top};
  const int namew=std::max(170,listW-360),datex=left+namew,typex=datex+158,sizex=typex+110;
  if(file_view==0){rect({left,y+151,listW,32},page);line(left,y+182,left+listW,y+182,edge);const struct Col{int x,width,id;const char*label;}columns[]={{left+43,namew-43,2110,"Name"},{datex,158,2111,"Date modified"},{typex,110,2112,"Type"},{sizex,82,2113,"Size"}};for(const auto&column:columns){add({column.x,y+151,column.width,31},column.id);small_text(column.x+3,y+160,column.label,muted(),6);if(file_sort==column.id-2110)small_text(column.x+column.width-18,y+158,file_sort_descending?"v":"^",muted(),6);}}
  if(file_error){small_text(left+24,top+30,file_error==EACCES||file_error==EPERM?"Access to this location is restricted.":"This location is unavailable.",ink());small_text(left+24,top+65,"Reconnect the drive or choose Refresh to try again.",muted(),6);}
  else if(file_matches.empty()){small_text(left+std::max(20,(listW-small_width(file_query[0]?"No items match your search.":"This folder is empty."))/2),top+45,file_query[0]?"No items match your search.":"This folder is empty.",muted());}
  for(int slot=0;slot<slots&&file_scroll+slot<int(file_matches.size());++slot){int index=file_matches[file_scroll+slot];auto&e=files[index];file_visible[slot]=index;++file_visible_count;Rect r{left+2+(slot%file_columns)*(listW/file_columns),top+(slot/file_columns)*rowheight,listW/file_columns-12,rowheight-2};int id=500+slot;add(r,id);
   const bool marked=std::find(file_selection.begin(),file_selection.end(),e.name)!=file_selection.end();
   if(marked||hover==id||focus==id)rect(r,marked?selected:over);
   int pad=file_select_mode?25:0;if(file_select_mode){rect({r.x+7,r.y+(r.h-14)/2,14,14},muted());rect({r.x+8,r.y+(r.h-14)/2+1,12,12},marked?blue:page);}
   const int kind=e.directory?5:SheetDocument::supports(e.name)?19:Paint::supports(e.name)?18:17;
   if(file_view==2){icon(r.x+12+pad,r.y+15,34,kind);small_fit(r.x+58+pad,r.y+13,r.w-68-pad,e.name,ink());small_fit(r.x+58+pad,r.y+37,r.w-68-pad,explorer_type(e),muted(),6);}
   else{icon(r.x+9+pad,r.y+(r.h-20)/2,20,kind);small_fit(r.x+41+pad,r.y+7,(file_view==0?namew:r.w)-48-pad,e.name,ink());if(file_view==0){explorer_metadata(*this,e);char modified[80]="";if(e.info.modified){std::time_t t=e.info.modified;if(auto value=std::localtime(&t))std::strftime(modified,sizeof(modified),"%m/%d/%Y %H:%M",value);}small_text(datex+3,r.y+8,modified,muted(),6);small_fit(typex+3,r.y+8,104,explorer_type(e),muted(),6);if(!e.directory&&e.info.regular)small_fit(sizex+3,r.y+8,78,explorer_size(e.info.size).c_str(),muted(),6);}}
  }
  file_scroll_track={left+listW-7,top,7,bottom-top};if(int(file_matches.size())>slots){int thumb=std::max(28,file_scroll_track.h*slots/int(file_matches.size())),travel=file_scroll_track.h-thumb;int st=file_scroll_track.y+travel*file_scroll/std::max(1,int(file_matches.size())-slots);round({file_scroll_track.x,st,5,thumb},2,muted());}
  if(file_preview_pane){int px=left+listW+9;line(px-9,y+150,px-9,bottom,edge);small_text(px+10,y+163,"Preview",ink());if(chosen){const auto&e=files[file_selected];small_fit(px+10,y+200,245,e.name,ink());small_text(px+10,y+230,explorer_type(e),muted(),6);
    std::string path=std::string(file_path)+"/"+e.name;if(file_preview_for!=path){file_preview_for=path;file_preview[0]=0;if(!e.directory){const auto&io=fileops::system_io();int fd=io.open(io.context,path.c_str(),false);if(fd>=0){auto n=io.read(io.context,fd,file_preview,sizeof(file_preview)-1);io.close(io.context,fd);if(n>0){file_preview[n]=0;for(int j=0;j<n;++j)if((unsigned char)file_preview[j]<32&&file_preview[j]!='\n'&&file_preview[j]!='\r'&&file_preview[j]!='\t'){file_preview[0]=0;break;}}}}}
    if(file_preview[0]){std::string line;int yy=y+272;for(const char*p=file_preview;*p&&yy<bottom-35;++p){if(*p=='\r')continue;if(*p=='\n'||small_width((line+*p).c_str(),6)>238){small_text(px+10,yy,line.c_str(),ink(),6);yy+=21;line.clear();if(*p=='\n')continue;}line+=*p;}if(!line.empty()&&yy<bottom-15)small_text(px+10,yy,line.c_str(),ink(),6);}else small_fit(px+10,y+273,240,"Open this item to view its contents.",muted(),6);
   }else small_text(px+10,y+213,"Select a file to preview.",muted(),6);
  }
 }
 // Explorer status bar.
 rect({x+1,y+h-34,w-2,33},page);line(x+1,y+h-34,x+w-2,y+h-34,edge);
 char status[160];size_t count=file_path[0]?file_matches.size():file_location==400?file_drives.size()+6:file_drives.size();std::snprintf(status,sizeof(status),"%zu%s items%s",count,file_truncated?"+":"",file_selection.empty()?"":"  |  ");std::string summary=status;if(!file_selection.empty())summary+=std::to_string(file_selection.size())+" selected";small_text(x+16,y+h-22,summary.c_str(),muted(),6);
 if(file_path[0]){Rect details{x+w-67,y+h-29,26,25},tiles{x+w-34,y+h-29,26,25};add(details,2120);add(tiles,2122);for(int i=0;i<3;++i)line(details.x+6,details.y+7+i*5,details.x+20,details.y+7+i*5,muted());rect({tiles.x+6,tiles.y+6,14,13},edge);rect({tiles.x+7,tiles.y+7,12,11},page);}
}
} // namespace ww
