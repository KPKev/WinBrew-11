// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/image_codec.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sys/types.h>
// Linux test runner only: force a real partial write without changing the
// production writer's API or simulating its implementation.
extern "C" ssize_t __real_write(int, const void *, size_t);
static bool fail_write = false;
static size_t budget = 0;
extern "C" ssize_t __wrap_write(int fd, const void *data, size_t count) {
  if (fail_write) {
    if (!budget) {
      errno = ENOSPC;
      return -1;
    }
    if (count > budget)
      count = budget;
    budget -= count;
  }
  return __real_write(fd, data, count);
}
int main() {
  const char *target = "build/paint-partial-copy.png";
  std::remove(target);
  uint32_t pixels[64];
  for (auto &p : pixels)
    p = 0xff675241;
  char error[180];
  fail_write = true;
  budget = 64;
  assert(!ww::image::write_new_png(target, pixels, 8, 8, error, sizeof(error)));
  assert(!std::filesystem::exists(target));
  assert(std::strstr(error, "Original unchanged") &&
         ww::image::error_code() == ENOSPC);
  fail_write = false;
  assert(ww::image::write_new_png(target, pixels, 8, 8, error, sizeof(error)));
  std::remove(target);
  std::puts(
      "PASS: partial disk-full write removes only its incomplete new copy");
}
