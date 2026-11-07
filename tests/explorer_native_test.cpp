#include "../src/explorer_directory.hpp"
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace {
unsigned char listing[2048];
size_t listing_size = 0;
int open_error = 0, read_error = 0, close_error = 0, read_count = 0, closed = 0;
char opened[1024]{};
void record(const char *name, uint8_t type, uint32_t inode = 1) {
  ww::directory::Record entry{};
  entry.inode = inode;
  entry.type = type;
  entry.name_length = uint8_t(std::strlen(name));
  entry.length = uint16_t((8 + entry.name_length + 1 + 3) & ~3u);
  std::snprintf(entry.name, sizeof(entry.name), "%s", name);
  std::memcpy(listing + listing_size, &entry, entry.length);
  listing_size += entry.length;
}
int denied(int error) { return int(0x80020000u | unsigned(error)); }
} // namespace
extern "C" int sceKernelOpen(const char *path, int, int) {
  if (open_error)
    return open_error;
  std::snprintf(opened, sizeof(opened), "%s", path);
  if (std::strstr(path, "/opaque"))
    return 10;
  read_count = 0;
  return 9;
}
extern "C" int sceKernelGetdents(int fd, char *bytes, int capacity) {
  assert(fd == 9 && capacity >= 65536);
  if (read_error)
    return read_error;
  if (read_count++)
    return 0;
  std::memcpy(bytes, listing, listing_size);
  return int(listing_size);
}
extern "C" int sceKernelClose(int fd) {
  assert(fd == 9 || fd == 10);
  ++closed;
  return close_error;
}
int main() {
  ww::Shell s;
  assert(s.init("assets"));
  record(".", 4);
  record("..", 4);
  record("zeta.txt", 8);
  record("folder", 4);
  record("deleted", 8, 0);
  s.file_open("/test");
  assert(!s.file_error && s.file_count == 2 && closed == 1);
  assert(s.files[0].directory && !std::strcmp(s.files[0].name, "folder"));
  s.file_select(0);
  assert(!std::strcmp(opened, "/test/folder"));
  assert(!std::strcmp(s.file_path, "/test/folder"));
  listing_size = 0;
  record("opaque",
         0); // Filesystems may omit d_type; open-as-directory resolves it.
  s.file_open("/test");
  assert(!s.file_error && s.file_count == 1 && s.files[0].directory);
  // An actual empty listing remains distinguishable from every failed stage.
  listing_size = 0;
  s.file_open("/empty");
  assert(!s.file_error && !s.file_count);
  open_error = denied(EPERM);
  s.file_open("/restricted");
  assert(s.file_error == EPERM && !s.file_count &&
         !std::strcmp(s.file_error_stage, "Open folder"));
  open_error = -1;
  errno = 0;
  s.file_refresh();
  assert(s.file_error == EIO);
  open_error = 0;
  read_error = denied(EACCES);
  s.file_refresh();
  assert(s.file_error == EACCES && !s.file_count &&
         !std::strcmp(s.file_error_stage, "Read folder"));
  read_error = 0;
  close_error = denied(EIO);
  s.file_refresh();
  assert(s.file_error == EIO &&
         !std::strcmp(s.file_error_stage, "Close folder"));
  close_error = 0;
  record("long-enough", 8);
  listing[4] =
      8; // A corrupt record length cannot advance into arbitrary memory.
  listing[5] = 0;
  s.file_refresh();
  assert(s.file_error == EIO && !s.file_count);
  size_t offset = 0;
  ww::directory::Record parsed{};
  assert(ww::directory::next(listing, 7, offset, parsed) ==
         ww::directory::Read::invalid);
  assert(ww::directory::error_or_io(0) == EIO);
  std::puts(
      "PASS: native directory ABI parsing, real folder navigation, empty "
      "listing, explicit permission/read/close errors and malformed data");
}
