// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
namespace ww {
namespace {
bool joined(char *out, size_t capacity, const char *root, const char *name) {
  const size_t n = std::strlen(root);
  const int result = std::snprintf(out, capacity, "%s%s%s", root,
                                   n && root[n - 1] == '/' ? "" : "/", name);
  return result > 0 && size_t(result) < capacity;
}
void report(Shell &shell, const char *value) {
  shell.dialog = true;
  shell.focus = 60;
  std::snprintf(shell.message, sizeof(shell.message), "%s", value);
}
const char *filename(const char *path) {
  const char *slash = std::strrchr(path, '/');
  return slash ? slash + 1 : path;
}
} // namespace
bool Shell::file_context_open() {
  if (window != 1 || dialog || notepad_prompt || paint.modal() ||
      file_transfer.busy() || file_properties)
    return false;
  if (file_games || !file_path[0])
    return file_window_bounds.has(mx, my);
  int selected = -1;
  Rect anchor{};
  if (input_device == InputDevice::controller && focus >= 500 && focus < 510) {
    selected = file_page * 10 + focus - 500;
    for (int i = 0; i < count; ++i)
      if (hits[i].id == focus)
        anchor = hits[i].r;
  } else {
    if (!file_window_bounds.has(mx, my))
      return false;
    if (!file_content_bounds.has(mx, my) || file_games || !file_path[0])
      return true;
    for (int i = count - 1; i >= 0; --i)
      if (hits[i].id >= 500 && hits[i].id < 510 && hits[i].r.has(mx, my)) {
        selected = file_page * 10 + hits[i].id - 500;
        break;
      }
  }
  if (selected >= file_count)
    selected = -1;
  file_selected = selected;
  file_context_item = selected >= 0;
  std::snprintf(file_context_folder, sizeof(file_context_folder), "%s",
                file_path);
  std::snprintf(file_context_path, sizeof(file_context_path), "%s", file_path);
  if (file_context_item) {
    if (!joined(file_context_path, sizeof(file_context_path), file_path,
                files[selected].name)) {
      report(*this, "This path is too long to open.");
      return true;
    }
    if (files[selected].directory)
      std::snprintf(file_context_folder, sizeof(file_context_folder), "%s",
                    file_context_path);
  }
  const int h = file_context_item ? 270 : 180;
  int x = mx, y = my;
  if (anchor.w) {
    x = anchor.x + 60;
    y = anchor.y + anchor.h / 2;
  }
  x = x < 0 ? 0 : x > W - 320 ? W - 320 : x;
  y = y < 0 ? 0 : y > H - 54 - h ? H - 54 - h : y;
  file_context_bounds = {x, y, 320, h};
  file_context = true;
  context = start = calendar = quick = tray = false;
  end_text();
  focus = file_context_item ? 800 : file_clipboard.ready ? 803 : 805;
  return true;
}
void Shell::file_action(int id) {
  if (id == 819)
    return;
  if (id == 807) {
    file_transfer.cancel();
    return;
  }
  if (id == 806) {
    file_properties = false;
    focus = -1;
    return;
  }
  if (file_transfer.busy() || file_properties)
    return;
  if (!file_context && !(id == 800 && file_context_path[0]))
    return;
  file_context = false;
  if (id == 802) {
    report(*this, "Cut is unavailable until a safe file move can be "
                  "guaranteed. Use Copy and Paste.");
    return;
  }
  if (id == 805) {
    file_refresh();
    return;
  }
  if (id == 800) {
    fileops::Info info{};
    if (fileops::inspect(file_context_path, info) < 0) {
      report(*this, "This file or folder could not be opened. Links and "
                    "special files are unsupported.");
      return;
    }
    if (info.directory)
      file_open(file_context_path);
    else if (!info.regular)
      report(*this, "Only regular files and folders can be opened here.");
    else if (Paint::supports(file_context_path))
      paint.open(*this, file_context_path);
    else
      notepad_open(file_context_path);
    return;
  }
  if (id == 801) {
    if (!file_clipboard.copy(file_context_path))
      report(*this, "Copy is unavailable for this file or location. Folders, "
                    "links and special files are unsupported.");
    return;
  }
  if (id == 803) {
    file_transfer.begin(file_clipboard, file_context_folder);
    file_transfer_seen = fileops::State::idle;
    file_operations_tick();
    return;
  }
  if (id == 804) {
    if (fileops::inspect(file_context_path, file_properties_info) < 0) {
      report(*this, "Properties are unavailable. The location may be "
                    "restricted, disconnected or a link.");
      return;
    }
    std::snprintf(file_properties_path, sizeof(file_properties_path), "%s",
                  file_context_path);
    file_properties = true;
    focus = 806;
  }
}
void Shell::file_operations_tick() {
  if (file_transfer.busy())
    file_transfer.step();
  if (file_transfer.state == file_transfer_seen)
    return;
  file_transfer_seen = file_transfer.state;
  if (file_transfer.busy() || file_transfer.state == fileops::State::idle)
    return;
  if (window == 1 && !file_games &&
      !std::strcmp(file_path, file_transfer.directory))
    file_refresh();
  if (file_transfer.state == fileops::State::complete) {
    for (int i = 0; i < file_count; ++i)
      if (!std::strcmp(files[i].name, filename(file_transfer.destination)))
        file_selected = i;
    return;
  }
  if (file_transfer.state == fileops::State::cancelled)
    report(*this, "Copy cancelled. The incomplete destination was kept. This "
                  "copy did not modify the source.");
  else if (file_transfer.destination_created)
    report(*this, "Copy failed. The incomplete destination was kept. This copy "
                  "did not modify the source.");
  else {
    dialog = true;
    focus = 60;
    std::snprintf(
        message, sizeof(message),
        "Copy could not start (%s, %d). Existing files are unchanged.",
        file_transfer.stage, file_transfer.error);
  }
}
void Shell::file_actions_ui() {
  if (file_context) {
    const auto r = file_context_bounds;
    panel(r);
    add(r, 819);
    auto item = [&](int row, int id, const char *label, bool enabled) {
      const Rect hit{r.x + 8, r.y + 8 + row * 44, r.w - 16, 42};
      if (enabled)
        button(hit, id, label);
      else
        text(hit.x + 26, hit.y + 12, label, 0, muted());
    };
    if (file_context_item) {
      const bool regular = file_selected >= 0 && file_selected < file_count &&
                           !files[file_selected].directory;
      item(0, 800, "Open", true);
      item(1, 801, "Copy", regular);
      item(2, 802, "Cut", false);
      item(3, 803, "Paste", file_clipboard.ready);
      item(4, 804, "Properties", true);
      text_fit(r.x + 17, r.y + 242, r.w - 34,
               "Cut: safe move not available yet", 0, muted());
    } else {
      item(0, 803, "Paste", file_clipboard.ready);
      item(1, 805, "Refresh", true);
      item(2, 804, "Properties", true);
      text_fit(r.x + 17, r.y + 150, r.w - 34,
               file_clipboard.ready ? "Paste creates a separate file"
                                    : "Copy a file to enable Paste",
               0, muted());
    }
  }
  if (file_properties) {
    rect({0, 0, W, H}, 0x38000000);
    const Rect r{500, 275, 920, 520};
    panel(r);
    text(r.x + 30, r.y + 24, "Properties", 2, ink());
    text_fit(r.x + 30, r.y + 80, r.w - 60, filename(file_properties_path), 1,
             ink());
    text(r.x + 30, r.y + 140, "Type", 0, muted());
    text(r.x + 200, r.y + 140,
         file_properties_info.directory ? "Folder"
         : file_properties_info.regular ? "File"
                                        : "Special file",
         0, ink());
    text(r.x + 30, r.y + 190, "Location", 0, muted());
    text_fit(r.x + 200, r.y + 190, r.w - 230, file_properties_path, 0, ink());
    char size[90];
    if (file_properties_info.directory)
      std::snprintf(size, sizeof(size), "Contents not counted");
    else
      std::snprintf(size, sizeof(size), "%llu bytes",
                    static_cast<unsigned long long>(file_properties_info.size));
    text(r.x + 30, r.y + 240, "Size", 0, muted());
    text(r.x + 200, r.y + 240, size, 0, ink());
    char modified[90] = "Unavailable";
    std::time_t time = std::time_t(file_properties_info.modified);
    if (const auto *when = std::localtime(&time))
      std::strftime(modified, sizeof(modified), "%Y-%m-%d  %H:%M:%S", when);
    text(r.x + 30, r.y + 290, "Modified", 0, muted());
    text(r.x + 200, r.y + 290, modified, 0, ink());
    text(r.x + 30, r.y + 340, "Attributes", 0, muted());
    text(r.x + 200, r.y + 340,
         file_properties_info.mode & 0222 ? "Writable permissions"
                                          : "Read-only permissions",
         0, ink());
    button({r.x + r.w - 174, r.y + r.h - 70, 144, 44}, 806, "Close");
  }
  if (file_transfer.busy()) {
    rect({0, 0, W, H}, 0x38000000);
    const Rect r{540, 350, 840, 360};
    panel(r);
    text(r.x + 30, r.y + 28,
         file_transfer.state == fileops::State::copying ? "Copying file"
                                                        : "Verifying copy",
         1, ink());
    text_fit(r.x + 30, r.y + 84, r.w - 60, filename(file_transfer.source), 0,
             ink());
    text_fit(r.x + 30, r.y + 125, r.w - 60, file_transfer.destination, 0,
             muted());
    const uint64_t done = file_transfer.state == fileops::State::copying
                              ? file_transfer.copied
                              : file_transfer.verified;
    const int percent =
        file_transfer.total ? int(done * 100 / file_transfer.total) : 100;
    round({r.x + 30, r.y + 183, r.w - 60, 12}, 6,
          dark ? 0xff514d48 : 0xffd8d2cc);
    round({r.x + 30, r.y + 183, (r.w - 60) * percent / 100, 12}, 6,
          accent_color());
    char progress[100];
    std::snprintf(progress, sizeof(progress), "%d%% | %llu / %llu bytes",
                  percent, static_cast<unsigned long long>(done),
                  static_cast<unsigned long long>(file_transfer.total));
    text(r.x + 30, r.y + 215, progress, 0, muted());
    text(r.x + 30, r.y + 255, "Existing files are never overwritten.", 0,
         muted());
    button({r.x + r.w - 180, r.y + r.h - 60, 150, 42}, 807, "Cancel copy");
  }
}
} // namespace ww
