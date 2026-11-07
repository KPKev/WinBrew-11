// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/paint.hpp"
#include "../src/shell.hpp"
#include "paint_test_helpers.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
static std::string read(const char *path) {
  FILE *f = std::fopen(path, "rb");
  assert(f);
  std::string text;
  char bytes[4096];
  size_t n;
  while ((n = std::fread(bytes, 1, sizeof(bytes), f)))
    text.append(bytes, n);
  std::fclose(f);
  return text;
}
int main() {
  namespace fs = std::filesystem;
  const char *folder = "build/paint-test-output";
  fs::create_directories(folder);
  for (const auto &entry : fs::directory_iterator(folder))
    fs::remove(entry.path());
  ww::Paint p;
  assert(ww::Paint::supports("photo.JPG") && ww::Paint::supports("image.png"));
  assert(!ww::Paint::supports("program.exe"));
  assert(p.load("tests/fixtures/paint-alpha.png"));
  assert(p.width == 7 && p.height == 5 && p.pixel(0, 0) == 0x800000ff);
  assert(ww::image::workspace_peak() <= ww::image::WorkspaceBytes);
  assert(p.load("tests/fixtures/paint-photo.jpg"));
  assert(p.width == 7 && p.height == 5);
  assert(p.load("tests/fixtures/paint-photo.bmp"));
  assert(p.pixel(0, 0) == 0xff0000ff);
  p.color = 0xff22aa44;
  p.brush = 2;
  p.stroke(1, 1, true);
  p.stroke(5, 3, false);
  assert(p.dirty() && p.pixel(1, 1) == p.color && p.pixel(5, 3) == p.color);
  p.undo();
  assert(!p.dirty() && p.pixel(1, 1) != p.color);
  p.redo();
  assert(p.dirty() && p.pixel(1, 1) == p.color);
  std::string original = read(p.path);
  assert(p.save_copy(folder, true));
  assert(!p.dirty() && fs::exists(p.last_saved));
  assert(read(p.path) == original);
  const std::string first_copy = read(p.last_saved);
  char error[180];
  uint32_t dummy = 0;
  assert(!ww::image::write_new_png(p.last_saved, &dummy, 1, 1, error,
                                   sizeof(error)));
  assert(read(p.last_saved) == first_copy);
  char saved[1024];
  std::snprintf(saved, sizeof(saved), "%s", p.last_saved);
  assert(p.load(saved) && p.pixel(1, 1) == 0xff22aa44);
  assert(p.save_copy(folder, true) && std::strcmp(saved, p.last_saved));
  p.stroke(2, 2, true);
  assert(!p.save_copy("build/paint-test-output/missing", true));
  assert(p.dirty() && p.pixel(2, 2) == p.color);
  const int width = p.width, height = p.height;
  const auto prior = p.pixel(0, 0);
  assert(!p.load("tests/fixtures/paint-invalid.png"));
  assert(p.width == width && p.height == height && p.pixel(0, 0) == prior &&
         p.dirty());
  ww::Shell s;
  assert(s.init("assets"));
  std::snprintf(s.preferences, sizeof(s.preferences), "%s/settings.ini",
                folder);
  p.show(s);
  p.open(s, "tests/fixtures/paint-alpha.png");
  assert(p.modal() && s.window == 5);
  p.activate(s, 742);
  assert(!p.modal() && p.dirty());
  assert(!p.can_close(s));
  p.activate(s, 740);
  finish_paint_save(p, s);
  assert(!p.modal() && !p.dirty() && s.window == 0);
  p.blank();
  p.show(s);
  auto r = p.canvas_rect({s.wx, s.wy, 1200, 780});
  const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  s.input_device = ww::InputDevice::mouse;
  assert(p.pointer(s, cx, cy, true, true));
  assert(p.pointer(s, cx + 20, cy, false, true));
  assert(p.dirty());
  p.pointer(s, cx + 20, cy, false, false);
  assert(!p.painting);
  s.start = true;
  assert(!p.pointer(s, cx + 30, cy, true, true));
  s.start = false;
  s.input_device = ww::InputDevice::controller;
  s.pad_buttons = 0x4000;
  s.mx = cx;
  s.my = cy;
  p.activate(s, 730);
  assert(p.painting);
  assert(p.pointer(s, cx + 40, cy, false, false));
  s.pad_buttons = 0;
  p.draw(s, {s.wx, s.wy, 1200, 780});
  assert(!p.painting);
  std::puts(
      "PASS: external PNG/JPEG/BMP decoding, bounded storage, brush/undo, PNG "
      "copy round-trip, original preservation, exclusive save, failure "
      "recovery, unsaved-close guard and mouse/controller strokes");
}
