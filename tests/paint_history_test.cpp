// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include "paint_test_helpers.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
int main() {
  namespace fs = std::filesystem;
  fs::create_directories("build/paint-history-output");
  for (const auto &entry : fs::directory_iterator("build/paint-history-output"))
    if (entry.is_regular_file())
      fs::remove(entry.path());
  ww::Shell s;
  assert(s.init("assets"));
  std::snprintf(s.preferences, sizeof(s.preferences),
                "build/paint-history-output/settings.ini");
  s.paint.open(s, "tests/fixtures/paint-4k.jpg");
  assert(!s.dialog && s.paint.width == 3840);
  uint32_t originals[20];
  s.paint.brush = 2;
  for (int i = 0; i < 20; ++i) {
    int x = 40 + i * 160, y = 500;
    originals[i] = s.paint.pixel(x, y);
    s.paint.color = 0xff002000u + uint32_t(i * 32);
    s.paint.stroke(x, y, true);
    s.paint.end_stroke();
    assert(s.paint.undo_cursor == i + 1);
  }
  assert(s.paint.undo_count == 20 &&
         s.paint.history_bytes <= ww::image::HistoryBytes);
  for (int i = 19; i >= 0; --i) {
    s.paint.undo();
    assert(s.paint.pixel(40 + i * 160, 500) == originals[i]);
  }
  assert(!s.paint.dirty() && s.paint.undo_cursor == 0);
  for (int i = 0; i < 20; ++i) {
    s.paint.redo();
    assert(s.paint.pixel(40 + i * 160, 500) == 0xff002000u + uint32_t(i * 32));
  }
  assert(s.paint.dirty() && s.paint.undo_cursor == 20);
  s.paint.color = 0xff336699;
  s.paint.stroke(3400, 500, true);
  s.paint.end_stroke();
  assert(s.paint.undo_count == 20);
  s.paint.undo();
  s.paint.undo();
  s.paint.undo();
  s.paint.color = 0xff997711;
  s.paint.stroke(3300, 700, true);
  s.paint.end_stroke();
  assert(s.paint.undo_cursor == 18 && s.paint.undo_count == 18);
  s.paint.redo();
  assert(s.paint.undo_cursor == 18);
  // Saving freezes the source and remains cancelable between bounded steps.
  assert(s.paint.begin_save(s, "build/paint-history-output", true));
  auto revision = s.paint.revision;
  auto pixel = s.paint.pixel(3300, 700);
  s.paint.stroke(3300, 700, true);
  s.paint.undo();
  s.paint.redo();
  assert(s.paint.revision == revision && s.paint.pixel(3300, 700) == pixel);
  s.paint.tick(s);
  s.paint.tick(s);
  assert(s.paint.saving() && s.paint.save_job.written < s.paint.save_job.total);
  char partial[1024];
  std::snprintf(partial, sizeof(partial), "%s", s.paint.save_job.path);
  s.paint.activate(s, 742);
  finish_paint_save(s.paint, s);
  assert(!fs::exists(partial) && s.paint.dirty() &&
         s.paint.pixel(3300, 700) == pixel);
  // Cancel must also stop the automatic next-name retry after a collision.
  FILE *existing = std::fopen(partial, "wb");
  assert(existing);
  std::fputs("EXISTING COPY", existing);
  std::fclose(existing);
  assert(s.paint.begin_save(s, "build/paint-history-output", true));
  s.paint.tick(s);
  assert(s.paint.save_job.phase == ww::image::SavePhase::cleanup &&
         s.paint.save_job.error == EEXIST);
  s.paint.activate(s, 742);
  finish_paint_save(s.paint, s);
  assert(!s.paint.saving() && s.paint.save_attempt == 1 &&
         fs::file_size(partial) == 13);
  std::remove(partial);
  s.dialog = false;
  // A full-image stroke consumes most of the budget; older entries are evicted.
  s.paint.brush = 9000;
  s.paint.color = 0xff102030;
  s.paint.stroke(1920, 1080, true);
  s.paint.end_stroke();
  assert(s.paint.undo_count <= 20 &&
         s.paint.history_bytes <= ww::image::HistoryBytes);
  s.paint.color = 0xff405060;
  s.paint.stroke(1920, 1080, true);
  s.paint.end_stroke();
  assert(s.paint.undo_count == 1 && std::strstr(s.paint.status, "budget"));
  s.paint.undo();
  assert(s.paint.pixel(3000, 1500) == 0xff102030);
  s.paint.redo();
  assert(s.paint.pixel(3000, 1500) == 0xff405060);
  s.paint.blank();
  s.paint.tool = ww::Paint::Tool::eraser;
  s.paint.brush = 6;
  s.paint.stroke(40, 40, true);
  s.paint.end_stroke();
  assert(!s.paint.dirty() && s.paint.undo_count == 0);
  std::puts("PASS: 20 full-res 4K undo/redo strokes, branch replacement, "
            "bounded history eviction, no-op stroke, frozen asynchronous save "
            "and cancel preservation");
}
