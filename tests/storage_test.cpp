// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
static unsigned array_allocations = 0;
void *operator new[](std::size_t size) {
  ++array_allocations;
  if (void *p = std::malloc(size))
    return p;
  throw std::bad_alloc();
}
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
alignas(64) static uint32_t frame[ww::W * ww::H];
alignas(64) static uint32_t background[ww::W * ww::H];
alignas(64) static unsigned char atlas[ww::AtlasBytes];
int main() {
  {
    ww::Shell s;
    assert(s.init("assets", frame, background, atlas));
    assert(!s.owns_buffers && s.pixels == frame && s.wallpaper == background &&
           s.font == atlas);
    s.draw();
    s.activate(40);
    s.draw();
    s.start = false;
    s.window = 2;
    for (int page = 0; page < 6; ++page) {
      s.settings_page = page;
      s.draw();
    }
    s.window = 3;
    s.address_edit = true;
    s.draw();
    assert(array_allocations == 0);
    assert(!s.init("assets", frame, background, atlas));
  }
  frame[0] = 123;
  assert(frame[0] == 123);
  ww::Shell partial;
  assert(!partial.init("assets", frame, nullptr, atlas));
  assert(partial.pixels == nullptr);
  std::puts("PASS: PS5-supplied buffers render with zero array allocations; "
            "ownership and invalid storage guarded");
}
