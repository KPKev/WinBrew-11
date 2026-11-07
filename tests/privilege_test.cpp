// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/privilege.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
static int raised = 0;
static bool busy = false;
static int fake_raise(int (*step)(void *), void *context) {
  if (busy) { errno = EBUSY; return -1; }
  ++raised;
  return step(context);
}
int main() {
  // Direct execution by default; errno from the step survives.
  assert(!ww::privilege::installed());
  int value = ww::privilege::run([] { return 7; });
  assert(value == 7);
  int r = ww::privilege::run([] { errno = ENOENT; return -1; });
  assert(r == -1 && errno == ENOENT);
  // Captures by reference work (pointer-returning calls use this form).
  const char *seen = nullptr;
  assert(ww::privilege::run([&] { seen = "ok"; return 0; }) == 0 && seen);
  // Installed raiser is used for every step.
  ww::privilege::install(fake_raise);
  assert(ww::privilege::installed());
  assert(ww::privilege::run([] { return 3; }) == 3 && raised == 1);
  // A busy backend refuses immediately instead of blocking.
  busy = true;
  bool ran = false;
  r = ww::privilege::run([&] { ran = true; return 0; });
  assert(r == -1 && errno == EBUSY && !ran);
  busy = false;
  ww::privilege::install(nullptr);
  assert(!ww::privilege::installed() && ww::privilege::run([] { return 1; }) == 1 && raised == 1);
  std::puts("PASS: privilege steps run directly by default, through the installed raiser, and refuse with EBUSY when busy");
}
