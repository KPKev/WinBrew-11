#include "../src/game_inventory.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
using namespace ww::games;
namespace {
std::string contents;
size_t offset = 0;
int open_error = 0, read_error = 0, close_error = 0, closes = 0;
int denied(int error) { return int(0x80020000u | unsigned(error)); }
}
extern "C" int sceKernelOpen(const char *, int flags, int mode) {
  assert(flags == 0 && mode == 0);
  offset = 0;
  return open_error ? open_error : 9;
}
extern "C" int64_t sceKernelRead(int fd, void *out, size_t size) {
  assert(fd == 9);
  if (read_error) return read_error;
  size_t count = std::min({size, contents.size() - offset, size_t(2)});
  std::memcpy(out, contents.data() + offset, count);
  offset += count;
  return int64_t(count);
}
extern "C" int sceKernelClose(int fd) {
  assert(fd == 9);
  ++closes;
  return close_error;
}
int main() {
  char out[6];
  contents = "12345";
  auto result = read_file(nullptr, "/read-only", out, sizeof(out));
  assert(result.status == ReadStatus::ok && result.bytes == 5 && !std::strcmp(out, "12345") && closes == 1);
  contents = "123456";
  result = read_file(nullptr, "/large", out, sizeof(out));
  assert(result.status == ReadStatus::too_large && result.error == EFBIG && closes == 2);
  contents.clear();
  result = read_file(nullptr, "/empty", out, sizeof(out));
  assert(result.status == ReadStatus::ok && !result.bytes && !out[0]);
  open_error = denied(ENOENT);
  result = read_file(nullptr, "/missing", out, sizeof(out));
  assert(result.status == ReadStatus::missing && result.error == ENOENT);
  open_error = denied(EPERM);
  result = read_file(nullptr, "/denied", out, sizeof(out));
  assert(result.status == ReadStatus::failed && result.error == EPERM);
  open_error = -1;
  result = read_file(nullptr, "/unknown-failure", out, sizeof(out));
  assert(result.status == ReadStatus::failed && result.error == EIO);
  open_error = 0;
  read_error = denied(EIO);
  result = read_file(nullptr, "/unreadable", out, sizeof(out));
  assert(result.status == ReadStatus::failed && result.error == EIO && closes == 4);
  read_error = 0;
  close_error = denied(EIO);
  result = read_file(nullptr, "/close-failure", out, sizeof(out));
  assert(result.status == ReadStatus::failed && result.error == EIO && closes == 5);
  std::puts("PASS: native read-only flags, partial reads, byte limits and explicit native open/read/close errors");
}
