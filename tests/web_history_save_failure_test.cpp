// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/web_history.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unistd.h>
static bool fail_write = false;
extern "C" ssize_t __real_write(int, const void *, size_t);
extern "C" ssize_t __wrap_write(int fd, const void *data, size_t size) {
  if (fail_write) { errno = ENOSPC; return -1; }
  return __real_write(fd, data, size);
}
int main() {
  std::filesystem::create_directories("build/web-history-tests");
  char path[256]; std::snprintf(path, sizeof(path), "build/web-history-tests/failure-%d.txt", int(getpid()));
  std::remove(path);
  ww::WebHistory h; assert(h.load(path));
  assert(h.remember("https://original.example"));
  fail_write = true;
  assert(!h.remember("https://new.example/path?secret=1") && h.error == ENOSPC && h.dirty);
  ww::WebHistory disk; assert(disk.load(path));
  assert(disk.count == 1 && !std::strcmp(disk.sites[0], "https://original.example"));
  assert(!h.clear() && h.error == ENOSPC && h.dirty);
  assert(disk.load(path) && disk.count == 1);
  fail_write = false;
  assert(h.clear() && !h.error && !h.dirty);
  assert(disk.load(path) && !disk.count);
  std::remove(path);
  std::puts("PASS: failed history writes and Clear preserve prior disk data and report failure");
}
