// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "image_codec.hpp"
#include <cstdint>
namespace ww {
struct Shell;
struct Rect;
struct Paint {
  enum class Tool { brush, eraser };
  enum class Pending { none, blank, open, close, action };
  int width = 800, height = 500, brush = 6;
  Tool tool = Tool::brush;
  uint32_t color = 0xff232323;
  bool initialized = false, painting = false, undo_ready = false,
       undo_is_redo = false;
  int previous_x = 0, previous_y = 0;
  uint64_t revision = 1, saved_revision = 1, undo_revision = 0,
           next_revision = 2;
  struct UndoStep {
    size_t offset = 0, bytes = 0;
    uint64_t before = 0, after = 0;
  };
  UndoStep steps[20]{};
  int undo_count = 0, undo_cursor = 0;
  size_t history_bytes = 0;
  bool stroke_open = false;
  int stroke_left = 0, stroke_top = 0, stroke_right = 0, stroke_bottom = 0;
  image::PngSaveJob save_job;
  bool save_continues_pending = false, save_to_files = false;
  int save_attempt = 0;
  char save_directory[1024]{}, save_stem[256]{};
  Pending pending = Pending::none;
  int pending_action = -1;
  char path[1024]{}, last_saved[1024]{}, pending_path[1024]{};
  char status[180] = "Open a PNG, JPEG or BMP, or start drawing.";
  static constexpr size_t FixedBytes =
      image::MaxPixels * 8 + image::WorkspaceBytes;
  static bool supports(const char *);
  bool dirty() const { return revision != saved_revision; }
  bool saving() const { return save_job.busy(); }
  bool modal() const { return pending != Pending::none || saving(); }
  // Raw model operations are exposed for meaningful portable verification.
  void blank();
  bool load(const char *);
  bool save_copy(const char *fallback_directory, bool to_files = false);
  void stroke(int x, int y, bool begin);
  void undo();
  void redo();
  void end_stroke();
  void clear_history();
  bool begin_save(Shell &, const char *fallback_directory,
                  bool to_files = false, bool continue_pending = false);
  void tick(Shell &);
  uint32_t pixel(int x, int y) const;
  // Shell integration. Window 5; actions 700..749.
  void open(Shell &, const char *);
  void show(Shell &);
  bool can_close(Shell &);
  // Call before any Shell action that navigates away or launches another app.
  // Returns true when the action has been deferred or an existing prompt owns
  // input. It also covers a minimized Paint canvas.
  bool defer_action(Shell &, int id);
  bool activate(Shell &, int);
  bool pointer(Shell &, int x, int y, bool down, bool held);
  void release() {
    end_stroke();
    painting = false;
  }
  void draw(Shell &, Rect);
  Rect canvas_rect(Rect) const;
  void finish_pending(Shell &, bool save);
};
} // namespace ww
