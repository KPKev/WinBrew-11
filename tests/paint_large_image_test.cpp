// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include "paint_test_helpers.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
static uint64_t hash(const char *path) {
  FILE *file = std::fopen(path, "rb");
  assert(file);
  uint64_t value = 14695981039346656037ull;
  unsigned char data[4096];
  size_t n;
  while ((n = std::fread(data, 1, sizeof(data), file)))
    for (size_t i = 0; i < n; ++i) {
      value ^= data[i];
      value *= 1099511628211ull;
    }
  std::fclose(file);
  return value;
}
int main() {
  namespace fs = std::filesystem;
  const fs::path folder = "build/paint-large-output";
  fs::create_directories(folder);
  for (const auto &entry : fs::directory_iterator(folder))
    if (entry.is_regular_file())
      fs::remove(entry.path());
  const std::string source = (folder / "USB Screenshot.jpg").generic_string();
  fs::copy_file("tests/fixtures/paint-4k.jpg", source);
  const uint64_t original = hash(source.c_str());
  ww::Shell s;
  assert(s.init("assets"));
  std::snprintf(s.preferences, sizeof(s.preferences),
                "build/paint-large-output/settings.ini");
  s.paint.open(s, source.c_str());
  assert(s.window == 5 && !s.dialog && s.paint.width == 3840 &&
         s.paint.height == 2160);
  auto old_pixel = s.paint.pixel(3800, 2100);
  s.paint.color = 0xff22bb88;
  s.paint.brush = 18;
  s.paint.stroke(3800, 2100, true);
  s.paint.stroke(3835, 2155, false);
  assert(s.paint.dirty() && s.paint.pixel(3800, 2100) == s.paint.color);
  s.paint.undo();
  assert(s.paint.pixel(3800, 2100) == old_pixel && !s.paint.dirty());
  s.paint.redo();
  s.paint.activate(s, 702); // Save beside the source, not in another folder.
  finish_paint_save(s.paint, s);
  assert(!s.dialog && !s.paint.dirty() && s.paint.last_saved[0]);
  assert(fs::path(s.paint.last_saved).parent_path() == folder);
  assert(hash(source.c_str()) == original);
  ww::image::EditingBuffer saved;
  int w = 0, h = 0;
  char error[200];
  assert(ww::image::read_editable(s.paint.last_saved, saved, w, h, error,
                                  sizeof(error)));
  assert(w == 3840 && h == 2160 &&
         saved.pixels[size_t(2100) * w + 3800] == s.paint.color);
  assert(ww::image::release_editable(saved) == 0);
  const auto prior = s.paint.pixel(3800, 2100);
  s.paint.open(s, "tests/fixtures/paint-invalid.png");
  assert(s.dialog && s.focus == 60 && s.paint.pixel(3800, 2100) == prior);
  assert(std::string(s.message).find("damaged") != std::string::npos);
  s.activate(60);
  s.paint.open(s, "tests/fixtures/paint-too-large.png");
  assert(s.dialog && std::string(s.message).find("4096") != std::string::npos);
  assert(s.paint.width == 3840 && s.paint.pixel(3800, 2100) == prior);
  s.activate(60);
  s.paint.blank();
  std::snprintf(s.preferences, sizeof(s.preferences),
                "build/no-such-paint-large/settings.ini");
  s.paint.activate(s, 709);
  finish_paint_save(s.paint, s);
  assert(s.dialog &&
         std::string(s.message).find("unavailable") != std::string::npos);
  s.paint.blank();
  std::puts("PASS: full-resolution 4K Paint strokes/undo, Save beside "
            "original, original hash preserved, 4K output pixels, and "
            "prominent open/save errors without blank replacement");
}
