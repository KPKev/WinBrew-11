// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/image_codec.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <map>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
namespace {
int coded(int error) { return int(uint32_t(0x80020000u) | unsigned(error)); }
struct File {
  bool writing;
  std::string path;
};
std::map<int, File> files;
std::map<void *, size_t> mappings;
size_t mapped = 0, peak_mapped = 0;
int map_attempts = 0, fail_map_at = 0, open_error = 0, create_error = 0;
bool short_io = false, fail_seek = false, fail_read = false, fail_sync = false,
     fail_close = false, corrupt_verify = false, fail_unmap = false;
int64_t write_budget = -1;
unsigned read_calls = 0, write_calls = 0;
unsigned io_calls = 0;
std::string saved_path;
void reset_faults() {
  fail_map_at = open_error = create_error = 0;
  fail_seek = fail_read = fail_sync = fail_close = corrupt_verify = fail_unmap =
      short_io = false;
  write_budget = -1;
}
std::string bytes(const char *path) {
  FILE *f = std::fopen(path, "rb");
  assert(f);
  std::string out;
  char b[4096];
  size_t n;
  while ((n = std::fread(b, 1, sizeof(b), f)))
    out.append(b, n);
  std::fclose(f);
  return out;
}
} // namespace
extern "C" int sceKernelOpen(const char *path, int flags, int mode) {
  ++io_calls;
  assert(flags == 0 || flags == (1 | 0x0200 | 0x0800));
  assert(!(flags & 0x0400)); // Never truncate an existing source or copy.
  bool writing = flags != 0;
  int failure = writing ? create_error : open_error;
  if (failure)
    return coded(failure);
  int fd = ::open(path, writing ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, mode);
  if (fd < 0)
    return coded(errno);
  files.emplace(fd, File{writing, path});
  if (writing)
    saved_path = path;
  return fd;
}
extern "C" int64_t sceKernelRead(int fd, void *data, size_t count) {
  ++io_calls;
  assert(files.count(fd) && !files.at(fd).writing);
  if (fail_read)
    return int64_t(coded(EIO));
  if (short_io && (++read_calls % 5) == 0)
    return int64_t(coded(EINTR));
  if (short_io)
    count = std::min(count, size_t(3));
  auto n = ::read(fd, data, count);
  if (n < 0)
    return int64_t(coded(errno));
  if (n > 0 && corrupt_verify && files.at(fd).path == saved_path) {
    static_cast<unsigned char *>(data)[0] ^= 1;
    corrupt_verify = false;
  }
  return n;
}
extern "C" int64_t sceKernelWrite(int fd, const void *data, size_t count) {
  ++io_calls;
  assert(files.count(fd) && files.at(fd).writing);
  if (write_budget == 0)
    return int64_t(coded(ENOSPC));
  if (short_io && (++write_calls % 5) == 0)
    return int64_t(coded(EINTR));
  if (short_io)
    count = std::min(count, size_t(7));
  if (write_budget > 0)
    count = std::min(count, size_t(write_budget));
  auto n = ::write(fd, data, count);
  if (n < 0)
    return int64_t(coded(errno));
  if (write_budget > 0)
    write_budget -= n;
  return n;
}
extern "C" int64_t sceKernelLseek(int fd, int64_t offset, int origin) {
  assert(files.count(fd));
  if (fail_seek)
    return int64_t(coded(EIO));
  auto result = ::lseek(fd, offset, origin);
  return result < 0 ? int64_t(coded(errno)) : result;
}
extern "C" int sceKernelFsync(int fd) {
  ++io_calls;
  assert(files.count(fd));
  return fail_sync ? coded(ENOSPC) : (::fsync(fd) == 0 ? 0 : coded(errno));
}
extern "C" int sceKernelClose(int fd) {
  ++io_calls;
  assert(files.count(fd));
  files.erase(fd);
  int code = ::close(fd);
  return fail_close ? coded(EIO) : code < 0 ? coded(errno) : 0;
}
extern "C" int sceKernelUnlink(const char *path) {
  ++io_calls;
  assert(path == saved_path); // Cleanup may touch only the copy we created.
  return ::unlink(path) == 0 ? 0 : coded(errno);
}
extern "C" int sceKernelMmap(void *address, size_t size, int prot, int flags,
                             int fd, int64_t offset, void **result) {
  assert(!address && prot == 3 && flags == 0x1002 && fd == -1 && offset == 0);
  assert(size && !(size & 0x3fff) &&
         size <= ww::image::TemporaryWorkspaceLimit);
  if (++map_attempts == fail_map_at)
    return coded(ENOMEM);
  void *memory = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED)
    return coded(errno);
  *result = memory;
  mappings[memory] = size;
  mapped += size;
  peak_mapped = std::max(mapped, peak_mapped);
  return 0;
}
extern "C" int sceKernelMunmap(void *address, size_t size) {
  assert(mappings.count(address) && mappings.at(address) == size);
  if (fail_unmap)
    return coded(EIO);
  int result = ::munmap(address, size);
  if (result < 0)
    return coded(errno);
  mappings.erase(address);
  mapped -= size;
  return 0;
}
int main() {
  namespace image = ww::image;
  std::filesystem::create_directories("build/paint-native-output");
  char error[200];
  int w = 3, h = 2;
  static uint32_t flat[image::MaxPixels];
  short_io = true;
  for (const char *path :
       {"tests/fixtures/paint-alpha.png", "tests/fixtures/paint-photo.jpg",
        "tests/fixtures/paint-photo.bmp"}) {
    assert(image::read(path, flat, w, h, error, sizeof(error)));
    assert(w == 7 && h == 5 && image::error_code() == 0);
  }
  reset_faults();
  flat[0] = 0xdeadbeef;
  fail_seek = true;
  assert(!image::read("tests/fixtures/paint-alpha.png", flat, w, h, error,
                      sizeof(error)));
  assert(flat[0] == 0xdeadbeef && image::error_code() == EIO && files.empty());
  reset_faults();
  open_error = EACCES;
  assert(!image::read("tests/fixtures/paint-alpha.png", flat, w, h, error,
                      sizeof(error)));
  assert(image::error_code() == EACCES && std::strstr(error, "access denied"));
  reset_faults();
  fail_read = true;
  assert(!image::read("tests/fixtures/paint-alpha.png", flat, w, h, error,
                      sizeof(error)));
  assert(flat[0] == 0xdeadbeef && image::error_code() == EIO);
  reset_faults();
  image::EditingBuffer photo;
  const std::string original = bytes("tests/fixtures/paint-4k.jpg");
  assert(image::read_editable("tests/fixtures/paint-4k.jpg", photo, w, h, error,
                              sizeof(error)));
  assert(w == 3840 && h == 2160 && photo.pixels && photo.undo);
  assert(mapped == photo.bytes && photo.bytes == 66355200 && files.empty());
  auto previous_pixel = photo.pixels[0];
  size_t previous_bytes = mapped;
  for (int failed_allocation = 1; failed_allocation <= 2; ++failed_allocation) {
    image::EditingBuffer rejected;
    int old_w = 17, old_h = 12;
    fail_map_at = map_attempts + failed_allocation;
    assert(!image::read_editable("tests/fixtures/paint-4k.png", rejected, old_w,
                                 old_h, error, sizeof(error)));
    assert(!rejected.pixels && old_w == 17 && old_h == 12 &&
           image::error_code() == ENOMEM);
    assert(mapped == previous_bytes && photo.pixels[0] == previous_pixel &&
           files.empty());
    reset_faults();
  }
  const int before_huge = map_attempts;
  image::EditingBuffer rejected;
  assert(!image::read_editable("tests/fixtures/paint-too-large.png", rejected,
                               w, h, error, sizeof(error)));
  assert(map_attempts == before_huge && image::error_code() == EFBIG);
  photo.pixels[0] = 0xff1199ee;
  const char *copy = "build/paint-native-output/4k-copy.png";
  std::remove(copy);
  assert(image::write_new_png(copy, photo.pixels, 3840, 2160, error,
                              sizeof(error)));
  assert(bytes("tests/fixtures/paint-4k.jpg") == original);
  assert(!image::write_new_png(copy, photo.pixels, 3840, 2160, error,
                               sizeof(error)));
  assert(image::error_code() == EEXIST);
  image::EditingBuffer reloaded;
  assert(image::read_editable(copy, reloaded, w, h, error, sizeof(error)));
  assert(w == 3840 && h == 2160 && reloaded.pixels[0] == 0xff1199ee);
  assert(image::release_editable(reloaded) == 0);
  for (int failure = 0; failure < 5; ++failure) {
    const char *partial = "build/paint-native-output/failed-copy.png";
    std::remove(partial);
    if (failure == 0)
      write_budget = 41;
    if (failure == 1)
      fail_sync = true;
    if (failure == 2)
      fail_close = true;
    if (failure == 3)
      corrupt_verify = true;
    if (failure == 4)
      create_error = EROFS;
    assert(!image::write_new_png(partial, photo.pixels, 7, 5, error,
                                 sizeof(error)));
    assert(!std::filesystem::exists(partial) && files.empty());
    assert(bytes("tests/fixtures/paint-4k.jpg") == original);
    reset_faults();
  }
  fail_unmap = true;
  assert(image::release_editable(photo) == -EIO && photo.pixels);
  reset_faults();
  assert(image::release_editable(photo) == 0 && !photo.pixels && !mapped &&
         files.empty());
  image::HistoryBuffer history;
  fail_map_at = map_attempts + 1;
  assert(image::allocate_history(history) == -ENOMEM && !history.data);
  reset_faults();
  assert(image::allocate_history(history) == 0 &&
         history.bytes == image::HistoryBytes);
  assert(image::release_history(history) == 0 && !history.data && !mapped);
  const char *async_path = "build/paint-native-output/incremental.png";
  std::remove(async_path);
  image::PngSaveJob job;
  assert(job.begin(async_path, flat, 128, 64));
  image::PngSaveJob blocked;
  assert(
      !blocked.begin("build/paint-native-output/blocked.png", flat, 128, 64));
  unsigned previous_progress = 0;
  bool saw_flush = false, saw_verify = false;
  while (job.busy()) {
    unsigned calls = io_calls;
    size_t before_write = job.written, before_verify = job.verified;
    job.step(1024);
    assert(io_calls - calls <= 32 && job.written - before_write <= 1024 &&
           job.verified - before_verify <= 1024);
    assert(job.progress() >= previous_progress);
    previous_progress = job.progress();
    saw_flush |= job.phase == image::SavePhase::flush;
    saw_verify |= job.phase == image::SavePhase::verify;
  }
  assert(job.phase == image::SavePhase::done && job.progress() == 100 &&
         saw_flush && saw_verify);
  for (bool cancel_during_verify : {false, true}) {
    std::remove(async_path);
    assert(job.begin(async_path, flat, 128, 64));
    while (job.busy() &&
           job.phase != (cancel_during_verify ? image::SavePhase::verify
                                              : image::SavePhase::rows))
      job.step(1024);
    assert(job.busy());
    job.cancel();
    job.step();
    assert(job.phase == image::SavePhase::canceled &&
           !std::filesystem::exists(async_path) && files.empty());
  }
  assert(peak_mapped <=
         image::TemporaryWorkspaceLimit + 2 * size_t(4096) * 2160 * 8);
  std::printf(
      "PASS: native image IO, full 4K JPEG/PNG editing and save round-trip, "
      "exclusive copies, bounded mapping failures, short IO/EINTR, "
      "seek/read/flush/close/verify errors, bounded incremental progress and "
      "cancel; peak mapped %zu bytes\n",
      peak_mapped);
}
