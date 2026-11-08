// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint.hpp"
#include "shell.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cmath>
namespace ww {
namespace {
// Fixed BSS storage. No image-sized arrays live on the stack or CRT heap.
alignas(16) uint32_t blank_canvas[image::MaxPixels];
alignas(16) uint32_t blank_history[image::MaxPixels];
uint32_t *canvas = blank_canvas, *history = blank_history;
image::EditingBuffer editing_buffer, pending_cleanup;
image::HistoryBuffer undo_storage;
constexpr uint32_t palette[] = {0xff232323, 0xff6a6a6a, 0xff2525e7, 0xff0068ed,
                                0xff00c5ff, 0xff52a521, 0xffe98e00, 0xffa64175,
                                0xffffffff, 0xffc6c6c6, 0xff9090ff, 0xffa3c4ff,
                                0xff9feeff, 0xffade4a8, 0xffffcd99, 0xffe5b6d9};
bool same_extension(const char *left, const char *right) {
  while (*left && *right) {
    char c = *left++;
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
    if (c != *right++)
      return false;
  }
  return !*left && !*right;
}
void set_status(Paint &paint, const char *value) {
  std::snprintf(paint.status, sizeof(paint.status), "%.*s",
                int(sizeof(paint.status) - 1), value);
}
void show_error(Shell &shell, const Paint &paint) {
  shell.dialog = true;
  shell.focus = 60;
  std::snprintf(shell.message, sizeof(shell.message), "%s", paint.status);
}
const char *basename(const char *path) {
  const char *slash = std::strrchr(path, '/');
  const char *back = std::strrchr(path, '\\');
  if (back && (!slash || back > slash))
    slash = back;
  return slash ? slash + 1 : path;
}
void preferences_directory(const Shell &shell, char *out, size_t capacity) {
  std::snprintf(out, capacity, "%s", shell.preferences);
  char *slash = std::strrchr(out, '/');
  char *back = std::strrchr(out, '\\');
  if (back && (!slash || back > slash))
    slash = back;
  if (slash) {
    if (slash == out)
      slash[1] = 0;
    else
      *slash = 0;
  } else
    std::snprintf(out, capacity, ".");
}
Rect window_rect(const Shell &shell) {
  return shell.maximized ? Rect{0, 0, W, H - 54}
                         : Rect{shell.wx, shell.wy, 1200, 780};
}
void stamp(Paint &paint, int x, int y) {
  const int radius = paint.tool == Paint::Tool::pencil ? 0 : paint.brush / 2;
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dx = -radius; dx <= radius; ++dx) {
      if (dx * dx + dy * dy > radius * radius)
        continue;
      int px = x + dx, py = y + dy;
      if (px >= 0 && py >= 0 && px < paint.width && py < paint.height)
        canvas[size_t(py) * paint.width + px] =
            paint.tool == Paint::Tool::eraser ? 0xffffffff : paint.color;
    }
}
} // namespace
bool Paint::supports(const char *path) {
  if (!path)
    return false;
  const char *ext = std::strrchr(path, '.');
  return ext && (same_extension(ext, ".png") || same_extension(ext, ".jpg") ||
                 same_extension(ext, ".jpeg") || same_extension(ext, ".bmp"));
}
void Paint::blank() {
  if (saving())
    return;
  const int released = image::release_editable(editing_buffer);
  if (released < 0) {
    std::snprintf(
        status, sizeof(status),
        "Could not release image memory (error %d). Your image is unchanged.",
        -released);
    return;
  }
  canvas = blank_canvas;
  history = blank_history;
  width = 800;
  height = 500;
  std::fill(canvas, canvas + size_t(width) * height, 0xffffffff);
  initialized = true;
  painting = undo_ready = undo_is_redo = false;
  revision = saved_revision = 1;
  next_revision = 2;
  path[0] = last_saved[0] = 0;
  clear_history();
  set_status(*this,
             "New canvas. Draw with a mouse or hold Cross and move the stick.");
}
bool Paint::load(const char *source) {
  if (saving()) {
    set_status(*this, "Wait for the current save or cancel it first.");
    return false;
  }
  end_stroke();
  if (!source || std::strlen(source) >= sizeof(path)) {
    set_status(*this, "The image path is too long.");
    return false;
  }
  char copy[sizeof(path)];
  std::snprintf(copy, sizeof(copy), "%s", source);
  if (image::release_editable(pending_cleanup) < 0) {
    set_status(*this, "Image memory is still busy. Close and reopen Paint; "
                      "previous image unchanged.");
    return false;
  }
  image::EditingBuffer replacement;
  int new_width = 0, new_height = 0;
  if (!image::read_editable(copy, replacement, new_width, new_height, status,
                            sizeof(status)))
    return false;
  const int released = image::release_editable(editing_buffer);
  if (released < 0) {
    if (image::release_editable(replacement) < 0)
      pending_cleanup = replacement;
    std::snprintf(
        status, sizeof(status),
        "Could not replace image memory (error %d). Previous image unchanged.",
        -released);
    return false;
  }
  editing_buffer = replacement;
  canvas = replacement.pixels;
  history = replacement.undo;
  width = new_width;
  height = new_height;
  std::snprintf(path, sizeof(path), "%s", copy);
  initialized = true;
  painting = undo_ready = undo_is_redo = false;
  revision = saved_revision = 1;
  next_revision = 2;
  last_saved[0] = 0;
  clear_history();
  std::snprintf(status, sizeof(status),
                "Opened at full resolution: %d x %d. Save copy preserves these "
                "dimensions.",
                width, height);
  return true;
}
bool Paint::save_copy(const char *fallback_directory, bool to_files) {
  if (saving()) {
    set_status(*this, "A copy is already being saved.");
    return false;
  }
  end_stroke();
  if (!initialized)
    blank();
  char directory[1024], stem[256], output[1300];
  const char *name = path[0] ? basename(path) : "Untitled";
  if (std::strlen(name) >= sizeof(stem)) {
    set_status(*this, "Use a shorter image filename before saving a copy.");
    return false;
  }
  std::snprintf(stem, sizeof(stem), "%s", name);
  char *dot = std::strrchr(stem, '.');
  if (dot && dot != stem)
    *dot = 0;
  if (path[0] && !to_files) {
    std::snprintf(directory, sizeof(directory), "%s", path);
    directory[name - path] = 0;
    size_t n = std::strlen(directory);
    if (n > 1 && (directory[n - 1] == '/' || directory[n - 1] == '\\'))
      directory[n - 1] = 0;
    if (!directory[0])
      std::snprintf(directory, sizeof(directory), ".");
  } else
    std::snprintf(directory, sizeof(directory), "%s",
                  fallback_directory && *fallback_directory ? fallback_directory
                                                            : ".");
  for (int attempt = 1; attempt <= 999; ++attempt) {
    int n = std::snprintf(output, sizeof(output), "%s/%s - Edited %03d.png",
                          directory, stem, attempt);
    if (n < 0 || n >= int(sizeof(last_saved))) {
      set_status(*this, "The save path is too long. Try Save to files.");
      return false;
    }
    // Exclusive creation is the only collision check. Native libc observation
    // must not turn an existing or unreadable file into an overwrite candidate.
    if (!image::write_new_png(output, canvas, width, height, status,
                              sizeof(status))) {
      if (image::error_code() == EEXIST)
        continue;
      return false;
    }
    std::memcpy(last_saved, output, size_t(n) + 1);
    saved_revision = revision;
    std::snprintf(status, sizeof(status), "Saved %s: %.100s",
                  (to_files || !path[0]) ? "in WinBrew 11 files"
                                         : "beside original",
                  basename(last_saved));
    return true;
  }
  set_status(*this,
             "No free copy filename is available. Choose another folder.");
  return false;
}
namespace {
bool next_save(Paint &paint) {
  char target[1024];
  int n =
      std::snprintf(target, sizeof(target), "%s/%s - Edited %03d.png",
                    paint.save_directory, paint.save_stem, paint.save_attempt);
  if (n < 0 || n >= int(sizeof(target))) {
    set_status(paint, "Save path is too long. Try Save to files.");
    return false;
  }
  if (!paint.save_job.begin(target, canvas, paint.width, paint.height)) {
    set_status(paint, paint.save_job.message[0]
                          ? paint.save_job.message
                          : "Another image save is already active.");
    return false;
  }
  return true;
}
} // namespace
bool Paint::begin_save(Shell &shell, const char *fallback, bool to_files,
                       bool continue_pending) {
  if (saving())
    return false;
  end_stroke();
  if (!initialized)
    blank();
  const char *name = path[0] ? basename(path) : "Untitled";
  if (std::strlen(name) >= sizeof(save_stem)) {
    set_status(*this, "Use a shorter image filename before saving a copy.");
    return false;
  }
  std::snprintf(save_stem, sizeof(save_stem), "%s", name);
  char *dot = std::strrchr(save_stem, '.');
  if (dot && dot != save_stem)
    *dot = 0;
  if (path[0] && !to_files) {
    std::snprintf(save_directory, sizeof(save_directory), "%s", path);
    save_directory[name - path] = 0;
    size_t n = std::strlen(save_directory);
    if (n > 1 &&
        (save_directory[n - 1] == '/' || save_directory[n - 1] == '\\'))
      save_directory[n - 1] = 0;
    if (!save_directory[0])
      std::snprintf(save_directory, sizeof(save_directory), ".");
  } else
    std::snprintf(save_directory, sizeof(save_directory), "%s",
                  fallback && *fallback ? fallback : ".");
  save_attempt = 1;
  save_to_files = to_files || !path[0];
  save_continues_pending = continue_pending;
  if (!next_save(*this))
    return false;
  shell.focus = 742;
  painting = false;
  return true;
}
void Paint::tick(Shell &shell) {
  if (!saving())
    return;
  save_job.step();
  if (saving())
    return;
  if (save_job.phase == image::SavePhase::failed && save_job.error == EEXIST &&
      !save_job.cancel_requested && save_attempt < 999) {
    ++save_attempt;
    if (next_save(*this))
      return;
  }
  if (save_job.phase == image::SavePhase::done) {
    std::snprintf(last_saved, sizeof(last_saved), "%s", save_job.path);
    saved_revision = revision;
    std::snprintf(status, sizeof(status), "Saved %s: %.100s",
                  save_to_files ? "in WinBrew 11 files" : "beside original",
                  basename(last_saved));
    bool follow = save_continues_pending;
    save_continues_pending = false;
    if (follow)
      finish_pending(shell, false);
    else
      shell.focus = -1;
  } else {
    set_status(*this, save_job.message[0]
                          ? save_job.message
                          : "The PNG copy was not saved. Drawing unchanged.");
    save_continues_pending = false;
    if (save_job.phase == image::SavePhase::failed && pending == Pending::none)
      show_error(shell, *this);
    else
      shell.focus = pending != Pending::none ? 742 : -1;
  }
}
void Paint::clear_history() {
  stroke_open = painting = undo_ready = undo_is_redo = false;
  undo_count = undo_cursor = 0;
  history_bytes = 0;
  image::release_history(
      undo_storage); // Failed release retains ownership for retry/reuse.
}
void Paint::stroke(int x, int y, bool begin) {
  if (saving())
    return;
  if (!initialized)
    blank();
  if (x < 0 || y < 0 || x >= width || y >= height)
    return;
  if (tool == Tool::picker) { color = pixel(x,y); return; }
  if (begin || !stroke_open) {
    end_stroke();
    int result = image::allocate_history(undo_storage);
    if (result < 0) {
      std::snprintf(
          status, sizeof(status),
          "Not enough undo memory (error %d). No pixels were changed.",
          -result);
      return;
    }
    std::memcpy(history, canvas, size_t(width) * height * 4);
    undo_revision = revision;
    revision = next_revision++;
    previous_x = origin_x = x;
    previous_y = origin_y = y;
    stroke_open = true;
    stroke_left = width;
    stroke_top = height;
    stroke_right = stroke_bottom = 0;
  }
  int radius = tool == Tool::pencil ? 0 : brush / 2;
  const bool shape = tool==Tool::line || tool==Tool::rectangle || tool==Tool::ellipse;
  if (shape) {
    // Restore only the earlier preview's bounds; history remains one undo step.
    for(int row=stroke_top;row<stroke_bottom;++row)
      if(stroke_right>stroke_left) std::memcpy(canvas+size_t(row)*width+stroke_left,history+size_t(row)*width+stroke_left,size_t(stroke_right-stroke_left)*4);
    previous_x=origin_x;previous_y=origin_y;
  }
  stroke_left =
      std::max(0, std::min(stroke_left, std::min(previous_x, x) - radius));
  stroke_top =
      std::max(0, std::min(stroke_top, std::min(previous_y, y) - radius));
  stroke_right = std::min(
      width, std::max(stroke_right, std::max(previous_x, x) + radius + 1));
  stroke_bottom = std::min(
      height, std::max(stroke_bottom, std::max(previous_y, y) + radius + 1));
  if (shape) {
    auto segment=[&](int ax,int ay,int bx,int by){int n=std::max(std::abs(bx-ax),std::abs(by-ay));for(int i=0;i<=n;++i)stamp(*this,ax+(bx-ax)*i/(n?n:1),ay+(by-ay)*i/(n?n:1));};
    if(tool==Tool::line)segment(origin_x,origin_y,x,y);
    else if(tool==Tool::rectangle){segment(origin_x,origin_y,x,origin_y);segment(x,origin_y,x,y);segment(x,y,origin_x,y);segment(origin_x,y,origin_x,origin_y);}
    else {double cx=(origin_x+x)*.5,cy=(origin_y+y)*.5,rx=std::abs(x-origin_x)*.5,ry=std::abs(y-origin_y)*.5;int n=std::max(24,int(std::max(rx,ry)*7));for(int i=0;i<=n;++i){double t=i*6.283185307179586/n;stamp(*this,int(std::lround(cx+rx*std::cos(t))),int(std::lround(cy+ry*std::sin(t))));}}
    return;
  }
  int dx = x - previous_x, dy = y - previous_y,
      steps = std::max(std::abs(dx), std::abs(dy));
  for (int at = 0; at <= steps; ++at)
    stamp(*this, previous_x + dx * at / (steps ? steps : 1),
          previous_y + dy * at / (steps ? steps : 1));
  previous_x = x;
  previous_y = y;
}
void Paint::end_stroke() {
  painting = false;
  if (!stroke_open)
    return;
  stroke_open = false;
  size_t needed = 0;
  for (int y = stroke_top; y < stroke_bottom; ++y) {
    int x = stroke_left;
    while (x < stroke_right) {
      if (canvas[size_t(y) * width + x] == history[size_t(y) * width + x]) {
        ++x;
        continue;
      }
      int start = x;
      while (x < stroke_right &&
             canvas[size_t(y) * width + x] != history[size_t(y) * width + x])
        ++x;
      needed += 8 + size_t(x - start) * 4;
    }
  }
  if (!needed) {
    revision = undo_revision;
    return;
  }
  if (needed > image::HistoryBytes) {
    undo_count = undo_cursor = 0;
    history_bytes = 0;
    undo_ready = false;
    set_status(*this, "This stroke exceeded the 32 MiB undo budget. History "
                      "cleared; original file unchanged.");
    return;
  }
  undo_count = undo_cursor;
  history_bytes =
      undo_cursor ? steps[undo_cursor - 1].offset + steps[undo_cursor - 1].bytes
                  : 0;
  bool evicted = false;
  while (undo_count >= 20 || needed > image::HistoryBytes - history_bytes) {
    size_t drop = steps[0].bytes;
    std::memmove(undo_storage.data, undo_storage.data + drop,
                 history_bytes - drop);
    history_bytes -= drop;
    for (int i = 1; i < undo_count; ++i) {
      steps[i - 1] = steps[i];
      steps[i - 1].offset -= drop;
    }
    --undo_count;
    --undo_cursor;
    evicted = true;
  }
  size_t offset = history_bytes;
  auto append = [&](uint32_t value) {
    std::memcpy(undo_storage.data + offset, &value, 4);
    offset += 4;
  };
  for (int y = stroke_top; y < stroke_bottom; ++y) {
    int x = stroke_left;
    while (x < stroke_right) {
      if (canvas[size_t(y) * width + x] == history[size_t(y) * width + x]) {
        ++x;
        continue;
      }
      int start = x;
      while (x < stroke_right &&
             canvas[size_t(y) * width + x] != history[size_t(y) * width + x])
        ++x;
      append(uint32_t(size_t(y) * width + start));
      append(uint32_t(x - start));
      for (int col = start; col < x; ++col)
        append(canvas[size_t(y) * width + col] ^
               history[size_t(y) * width + col]);
    }
  }
  steps[undo_count++] = {history_bytes, needed, undo_revision, revision};
  undo_cursor = undo_count;
  history_bytes = offset;
  undo_ready = true;
  undo_is_redo = false;
  if (evicted)
    std::snprintf(status, sizeof(status),
                  "Undo: %d of 20 strokes available (32 MiB memory budget).",
                  undo_cursor);
}
namespace {
bool apply_step(Paint &paint, const Paint::UndoStep &step) {
  size_t offset = step.offset, end = step.offset + step.bytes;
  if (!undo_storage.data || end > paint.history_bytes)
    return false;
  // Validate the entire record before changing a single pixel.
  while (offset < end) {
    uint32_t start = 0, count = 0;
    if (end - offset < 8)
      return false;
    std::memcpy(&start, undo_storage.data + offset, 4);
    std::memcpy(&count, undo_storage.data + offset + 4, 4);
    offset += 8;
    if (!count || size_t(start) + count > size_t(paint.width) * paint.height ||
        size_t(count) > ((end - offset) / 4))
      return false;
    offset += size_t(count) * 4;
  }
  if (offset != end)
    return false;
  offset = step.offset;
  while (offset < end) {
    uint32_t start, count;
    std::memcpy(&start, undo_storage.data + offset, 4);
    std::memcpy(&count, undo_storage.data + offset + 4, 4);
    offset += 8;
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t value;
      std::memcpy(&value, undo_storage.data + offset, 4);
      offset += 4;
      canvas[start + i] ^= value;
    }
  }
  return true;
}
} // namespace
void Paint::undo() {
  if (saving())
    return;
  end_stroke();
  if (!undo_cursor)
    return;
  if (!apply_step(*this, steps[undo_cursor - 1])) {
    set_status(*this, "Undo history unavailable. Image unchanged.");
    return;
  }
  revision = steps[--undo_cursor].before;
  undo_ready = undo_cursor > 0;
  undo_is_redo = false;
  std::snprintf(status, sizeof(status),
                "Undo: %d   Redo: %d   |   Up to 20 strokes", undo_cursor,
                undo_count - undo_cursor);
}
void Paint::redo() {
  if (saving())
    return;
  end_stroke();
  if (undo_cursor >= undo_count)
    return;
  if (!apply_step(*this, steps[undo_cursor])) {
    set_status(*this, "Redo history unavailable. Image unchanged.");
    return;
  }
  revision = steps[undo_cursor++].after;
  undo_ready = undo_cursor > 0;
  undo_is_redo = false;
  std::snprintf(status, sizeof(status),
                "Undo: %d   Redo: %d   |   Up to 20 strokes", undo_cursor,
                undo_count - undo_cursor);
}
uint32_t Paint::pixel(int x, int y) const {
  return initialized && x >= 0 && y >= 0 && x < width && y < height
             ? canvas[size_t(y) * width + x]
             : 0;
}
Rect Paint::canvas_rect(Rect wr) const {
  const int available_w = wr.w - 52, available_h = wr.h - 312;
  int scale = std::min(
      1000, std::min(available_w * 1000 / width, available_h * 1000 / height));
  int w = std::max(1, width * scale / 1000),
      h = std::max(1, height * scale / 1000);
  return {wr.x + (wr.w - w) / 2, wr.y + 260 + (available_h - h) / 2, w, h};
}
void Paint::show(Shell &shell) {
  end_stroke();
  if (!initialized)
    blank();
  painting = false;
  shell.end_text();
  shell.start = shell.context = shell.quick = shell.tray = shell.calendar =
      false;
  shell.window = 5;
  shell.focus = -1;
}
void Paint::open(Shell &shell, const char *source) {
  if (!source || std::strlen(source) >= sizeof(pending_path)) {
    show(shell);
    set_status(*this, "The image path is too long.");
    show_error(shell, *this);
    return;
  }
  char copy[sizeof(pending_path)];
  std::snprintf(copy, sizeof(copy), "%s", source);
  show(shell);
  if (dirty()) {
    std::snprintf(pending_path, sizeof(pending_path), "%s", copy);
    pending = Pending::open;
    set_status(*this, "Your drawing has unsaved changes.");
  } else if (!load(copy))
    show_error(shell, *this);
}
bool Paint::can_close(Shell &shell) {
  painting = false;
  if (!dirty())
    return true;
  pending = Pending::close;
  set_status(*this, "Your drawing has unsaved changes.");
  shell.focus = 742;
  return false;
}
bool Paint::defer_action(Shell &shell, int id) {
  end_stroke();
  if (modal())
    return true;
  if (!dirty()) {
    if ((id == 93 && shell.window == 5) || id == 13 || id == 30 || id == 12 ||
        id == 31) {
      blank();
      if (editing_buffer.pixels) {
        show_error(shell, *this);
        return true;
      }
    }
    return false;
  }
  show(shell);
  pending = Pending::action;
  pending_action = id;
  set_status(*this, "Your drawing has unsaved changes.");
  shell.focus = 742;
  return true;
}
void Paint::finish_pending(Shell &shell, bool save) {
  char folder[1024];
  preferences_directory(shell, folder, sizeof(folder));
  if (save) {
    begin_save(shell, folder, false, true);
    return;
  }
  const auto requested = pending;
  const int requested_action = pending_action;
  pending = Pending::none;
  pending_action = -1;
  if (requested == Pending::blank)
    blank();
  else if (requested == Pending::open) {
    if (!load(pending_path))
      show_error(shell, *this);
  } else if (requested == Pending::close) {
    blank();
    if (editing_buffer.pixels) {
      show_error(shell, *this);
      return;
    }
    shell.window = shell.restored = 0;
  } else if (requested == Pending::action) {
    if (!save)
      blank();
    shell.activate(requested_action);
  }
  shell.focus = -1;
}
bool Paint::activate(Shell &shell, int id) {
  if (id < 700 || id > 749)
    return false;
  if (saving()) {
    if (id == 742)
      save_job.cancel();
    return true;
  }
  if (modal()) {
    if (id == 740 || id == 741)
      finish_pending(shell, id == 740);
    else if (id == 742) {
      pending = Pending::none;
      pending_action = -1;
      shell.focus = -1;
    }
    return true;
  }
  end_stroke();
  if (id == 700) {
    if (dirty()) {
      pending = Pending::blank;
      set_status(*this, "Your drawing has unsaved changes.");
      shell.focus = 742;
    } else
      blank();
  } else if (id == 701) {
    shell.file_location_open(400);
    set_status(*this, "Choose an image in File Explorer to open it in Paint.");
  } else if (id == 702 || id == 709) {
    char folder[1024];
    preferences_directory(shell, folder, sizeof(folder));
    if (!begin_save(shell, folder, id == 709))
      show_error(shell, *this);
  } else if (id == 703)
    undo();
  else if (id == 731)
    redo();
  else if (id >= 732 && id <= 736) {
    end_stroke();tool = id==732?Tool::line:id==733?Tool::rectangle:id==734?Tool::ellipse:id==735?Tool::picker:Tool::pencil;
  }
  else if (id == 704 || id == 705)
    tool = id == 704 ? Tool::brush : Tool::eraser;
  else if (id >= 706 && id <= 708)
    brush = id == 706 ? 2 : id == 707 ? 6 : 18;
  else if (id >= 710 && id <= 725)
    color = palette[id - 710];
  else if (id == 730) {
    auto r = canvas_rect(window_rect(shell));
    if (!r.has(shell.mx, shell.my)) {
      shell.mx = r.x + r.w / 2;
      shell.my = r.y + r.h / 2;
    }
    pointer(shell, shell.mx, shell.my, true, true);
  }
  return true;
}
bool Paint::pointer(Shell &shell, int x, int y, bool down, bool held) {
  if (shell.window != 5 || modal() || shell.dialog || shell.start ||
      shell.context || shell.quick || shell.tray || shell.calendar ||
      shell.touch_keyboard || shell.drag) {
    end_stroke();
    return false;
  }
  if (shell.input_device == InputDevice::controller)
    held =
        (shell.pad_buttons & (shell.pad_swap_confirm ? 0x2000u : 0x4000u)) != 0;
  auto r = canvas_rect(window_rect(shell));
  if (!r.has(x, y)) {
    end_stroke();
    return false;
  }
  if (down || (painting && held)) {
    shell.focus = -1;
    stroke((x - r.x) * width / r.w, (y - r.y) * height / r.h,
           down || !painting);
    painting = stroke_open;
    if (!painting)
      show_error(shell, *this);
    return true;
  }
  if (!held)
    end_stroke();
  return false;
}
void Paint::draw(Shell &shell, Rect wr) {
  if (!initialized)
    blank();
  if (shell.input_device == InputDevice::controller &&
      !(shell.pad_buttons & (shell.pad_swap_confirm ? 0x2000u : 0x4000u)))
    end_stroke();
  const int x = wr.x, y = wr.y;
  shell.rect({x + 1, y + 43, wr.w - 2, wr.h - 44},
             shell.dark ? 0xff272727 : 0xfff3f3f3);
  shell.button({x + 16, y + 54, 74, 38}, 700, "New");
  shell.button({x + 94, y + 54, 80, 38}, 701, "Open");
  shell.button({x + 181, y + 54, 116, 38}, 702, "Save copy");
  shell.button({x + 306, y + 54, 132, 38}, 709, "Save to files");
  shell.button({x + 454, y + 54, 84, 38}, 703, "Undo");
  shell.button({x + 540, y + 54, 84, 38}, 731, "Redo");
  shell.text_fit(x + 650, y + 66, wr.w - 676,
                 path[0] ? basename(path) : "Untitled", 0, shell.muted());
  shell.round({x + 12, y + 104, wr.w - 24, 138}, 8,
              shell.dark ? 0xff303030 : 0xffffffff);
  auto choice = [&](Rect r, int id, const char *label, bool chosen) {
    if (chosen)
      shell.round(r, 5, shell.dark ? 0xff51463c : 0xfff4e9de);
    shell.button(r, id, label);
    if (chosen)
      shell.round({r.x + 12, r.y + r.h - 3, r.w - 24, 2}, 1,
                  shell.accent_color());
  };
  choice({x + 25, y + 118, 90, 45}, 704, "Brush", tool == Tool::brush);
  choice({x + 25, y + 177, 90, 45}, 705, "Eraser", tool == Tool::eraser);
  shell.line(x + 133, y + 120, x + 133, y + 225, shell.muted());
  shell.text(x + 157, y + 121, "Brush size", 0, shell.muted());
  const int sizes[] = {2, 6, 18};
  for (int i = 0; i < 3; ++i) {
    Rect r{x + 153 + i * 63, y + 164, 53, 48};
    choice(r, 706 + i, "", brush == sizes[i]);
    const int diameter = i == 0 ? 4 : i == 1 ? 9 : 18;
    shell.round({r.x + (r.w - diameter) / 2, r.y + (r.h - diameter) / 2,
                 diameter, diameter},
                diameter / 2, shell.ink());
  }
  shell.line(x + 365, y + 120, x + 365, y + 225, shell.muted());
  shell.text(x + 390, y + 121, "Colors", 0, shell.muted());
  shell.round({x + 390, y + 163, 43, 43}, 21, shell.muted());
  shell.round({x + 394, y + 167, 35, 35}, 17, color);
  for (int i = 0; i < 16; ++i) {
    Rect r{x + 455 + (i % 8) * 38, y + 143 + (i / 8) * 39, 31, 31};
    shell.add(r, 710 + i);
    bool chosen = color == palette[i];
    shell.round(r, 15, chosen ? shell.accent_color() : shell.muted());
    shell.round({r.x + 3, r.y + 3, 25, 25}, 12, palette[i]);
  }
  shell.text(x + 796, y + 115, "Shapes", 0, shell.muted());
  choice({x+786,y+144,100,36},732,"Line",tool==Tool::line);
  choice({x+890,y+144,128,36},733,"Rectangle",tool==Tool::rectangle);
  choice({x+1022,y+144,125,36},734,"Ellipse",tool==Tool::ellipse);
  choice({x+786,y+189,100,36},736,"Pencil",tool==Tool::pencil);
  choice({x+890,y+189,170,36},735,"Color picker",tool==Tool::picker);

  char available[70];
  std::snprintf(available, sizeof(available),
                "Undo %d  |  Redo %d  |  Limit 20", undo_cursor,
                undo_count - undo_cursor);

  const auto r = canvas_rect(wr);
  shell.round({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, 2, shell.muted());
  shell.add(r, 730);
  for (int py = std::max(0, r.y); py < std::min(H, r.y + r.h); ++py)
    for (int px = std::max(0, r.x); px < std::min(W, r.x + r.w); ++px) {
      int ix = (px - r.x) * width / r.w, iy = (py - r.y) * height / r.h;
      uint32_t c = canvas[size_t(iy) * width + ix], alpha = c >> 24;
      uint32_t checker = ((px - r.x) / 12 + (py - r.y) / 12) % 2 ? 223 : 248;
      uint32_t result = 0xff000000;
      for (int shift = 0; shift < 24; shift += 8)
        result |= (((c >> shift & 255) * alpha + checker * (255 - alpha)) / 255)
                  << shift;
      shell.pixels[size_t(py) * W + px] = result;
    }
  if (shell.focus == 730)
    shell.rect({r.x, r.y, r.w, 2}, shell.accent_color());
  char details[100];
  std::snprintf(details, sizeof(details), "%d x %d px   |   %d%%   |   %s",
                width, height, r.w * 100 / width,
                dirty() ? "Unsaved changes" : "Ready");
  shell.text(x + 18, y + wr.h - 35, details, 0, shell.muted());
  shell.text_fit(x + 440, y + wr.h - 35, wr.w - 462, status, 0, shell.muted());
  if (saving()) {
    shell.rect(wr, 0x50000000);
    Rect box{x + (wr.w - 740) / 2, y + (wr.h - 330) / 2, 740, 330};
    shell.panel(box);
    shell.add(wr, 89);
    shell.text(box.x + 28, box.y + 26,
               save_job.cancel_requested ? "Canceling save" : save_job.stage(),
               2, shell.ink());
    shell.text_fit(box.x + 28, box.y + 88, box.w - 56, basename(save_job.path),
                   0, shell.ink());
    shell.text_fit(box.x + 28, box.y + 124, box.w - 56, save_directory, 0,
                   shell.muted());
    shell.round({box.x + 28, box.y + 177, box.w - 56, 10}, 5, shell.muted());
    shell.round({box.x + 28, box.y + 177,
                 (box.w - 56) * int(save_job.progress()) / 100, 10},
                5, shell.accent_color());
    char progress[80];
    std::snprintf(progress, sizeof(progress),
                  "%u%%  |  Original file unchanged", save_job.progress());
    shell.text(box.x + 28, box.y + 208, progress, 0, shell.muted());
    shell.button({box.x + box.w - 180, box.y + 266, 150, 42}, 742,
                 save_job.cancel_requested ? "Canceling..." : "Cancel");
  } else if (modal()) {
    shell.rect(wr, 0x50000000);
    const Rect box{x + (wr.w - 650) / 2, y + (wr.h - 270) / 2, 650, 270};
    shell.panel(box);
    shell.add(wr, 89);
    shell.text(box.x + 28, box.y + 26, "Save your changes?", 2, shell.ink());
    shell.text(box.x + 28, box.y + 87,
               "Keep your drawing as a new PNG before continuing.", 0,
               shell.muted());
    shell.text_fit(box.x + 28, box.y + 125, box.w - 56, status, 0,
                   shell.muted());
    Rect save{box.x + 25, box.y + 194, 174, 45};
    shell.add(save, 740);
    shell.round(save, 5, shell.accent_color());
    shell.center(save.x, save.y + 11, save.w, "Save copy", 0, 0xffffffff);
    if (shell.focus == 740)
      shell.rect({save.x + 8, save.y + save.h - 5, save.w - 16, 2}, 0xffffffff);
    shell.round({box.x + 215, box.y + 194, 174, 45}, 5,
                shell.dark ? 0xff48423c : 0xffeee9e3);
    shell.round({box.x + 405, box.y + 194, 216, 45}, 5,
                shell.dark ? 0xff48423c : 0xffeee9e3);
    shell.button({box.x + 215, box.y + 194, 174, 45}, 741, "Don't save");
    shell.button({box.x + 405, box.y + 194, 216, 45}, 742, "Cancel");
  }
}
} // namespace ww
