// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/web_history.hpp"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
namespace {
int open_failure = 0, write_failure = 0, sync_failure = 0, rename_failure = 0, read_failure = 0;
int opens = 0, reads = 0, writes = 0, syncs = 0, closes = 0, renames = 0, unlinks = 0;
bool short_writes = false;
int failure(int error) { return int32_t(0x80020000u | unsigned(error)); }
int native_result(int result) { return result < 0 ? failure(errno ? errno : EIO) : result; }
}
extern "C" int sceKernelOpen(const char *name, int flags, int mode) {
  ++opens;
  if (open_failure) return failure(open_failure);
  return native_result(::open(name, flags, mode));
}
extern "C" int64_t sceKernelRead(int fd, void *data, size_t size) {
  ++reads;
  if (read_failure) return failure(read_failure);
  const auto result = ::read(fd, data, size);
  return result < 0 ? failure(errno) : result;
}
extern "C" int64_t sceKernelWrite(int fd, const void *data, size_t size) {
  ++writes;
  if (write_failure) return failure(write_failure);
  if (short_writes && size > 3) size = 3;
  const auto result = ::write(fd, data, size);
  return result < 0 ? failure(errno) : result;
}
extern "C" int sceKernelFsync(int fd) {
  ++syncs;
  return sync_failure ? failure(sync_failure) : native_result(::fsync(fd));
}
extern "C" int sceKernelClose(int fd) { ++closes; return native_result(::close(fd)); }
extern "C" int sceKernelRename(const char *from, const char *to) {
  ++renames;
  return rename_failure ? failure(rename_failure) : native_result(std::rename(from, to));
}
extern "C" int sceKernelUnlink(const char *name) { ++unlinks; return native_result(::unlink(name)); }
int main() {
  std::filesystem::create_directories("build/web-history-tests");
  char path[256]; std::snprintf(path, sizeof(path), "build/web-history-tests/native-%d.txt", int(getpid()));
  std::remove(path);
  ww::WebHistory h;
  assert(h.load(path) && !h.count);
  short_writes = true;
  assert(h.remember("https://original.example/private?secret=1") && !h.error);
  assert(opens >= 3 && reads > 0 && writes > 1 && syncs == 1 && renames == 1);
  ww::WebHistory disk;
  assert(disk.load(path) && disk.count == 1 && !std::strcmp(disk.sites[0], "https://original.example"));
  const int old_renames = renames;
  write_failure = ENOSPC;
  assert(!h.remember("https://new.example") && h.error == ENOSPC && h.dirty);
  write_failure = 0;
  assert(disk.load(path) && disk.count == 1 && renames == old_renames && unlinks == 1);
  sync_failure = EIO;
  assert(!h.save() && h.error == EIO && h.dirty);
  sync_failure = 0;
  assert(disk.load(path) && disk.count == 1 && renames == old_renames);
  rename_failure = EACCES;
  assert(!h.save() && h.error == EACCES && h.dirty);
  rename_failure = 0;
  assert(disk.load(path) && disk.count == 1);
  open_failure = EACCES;
  assert(!disk.load(path) && disk.error == EACCES && disk.count == 1);
  assert(!h.clear() && h.error == EACCES && h.dirty);
  open_failure = 0;
  assert(disk.load(path) && disk.count == 1);
  read_failure = EIO;
  assert(!disk.load(path) && disk.error == EIO && disk.count == 1);
  read_failure = 0;
  assert(h.save() && !h.error && !h.dirty);
  assert(disk.load(path) && disk.count == 0);
  assert(closes > 0);
  std::remove(path);
  std::puts("PASS: kernel history IO, partial writes, native permission/read/sync/replace errors and atomic preservation");
}
