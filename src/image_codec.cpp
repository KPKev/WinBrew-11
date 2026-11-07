// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_codec.hpp"
#include "privilege.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#define WW_OPEN _open
#define WW_WRITE _write
#define WW_READ _read
#define WW_SEEK _lseeki64
#define WW_SYNC _commit
#define WW_CLOSE _close
#define WW_UNLINK _unlink
#else
#include <sys/mman.h>
#include <unistd.h>
#define WW_OPEN open
#define WW_WRITE write
#define WW_READ read
#define WW_SEEK lseek
#define WW_SYNC fsync
#define WW_CLOSE close
#define WW_UNLINK unlink
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
extern "C" {
int sceKernelOpen(const char *, int, int);
int64_t sceKernelRead(int, void *, size_t);
int64_t sceKernelWrite(int, const void *, size_t);
int64_t sceKernelLseek(int, int64_t, int);
int sceKernelFsync(int);
int sceKernelClose(int);
int sceKernelUnlink(const char *);
int sceKernelMmap(void *, size_t, int, int, int, int64_t, void **);
int sceKernelMunmap(void *, size_t);
}
#ifdef __PROSPERO__
static_assert(O_RDONLY == 0 && O_WRONLY == 1 && O_CREAT == 0x0200 &&
              O_EXCL == 0x0800);
#endif
#endif
namespace {
alignas(16) unsigned char workspace[ww::image::WorkspaceBytes];
unsigned char *workspace_data = workspace;
size_t workspace_capacity = sizeof(workspace);
struct Block {
  size_t offset = 0, size = 0;
  int state = 0; // 0 unused, 1 free, 2 allocated
};
Block blocks[128];
size_t live_bytes = 0, peak_bytes = 0;
bool workspace_exhausted = false;
void reset_workspace() {
  for (auto &b : blocks)
    b = {};
  blocks[0] = {0, workspace_capacity, 1};
  live_bytes = peak_bytes = 0;
  workspace_exhausted = false;
}
void *bounded_alloc(size_t size) {
  if (!size || size > workspace_capacity - 15) {
    workspace_exhausted = true;
    return nullptr;
  }
  size = (size + 15) & ~size_t(15);
  for (auto &b : blocks) {
    if (b.state != 1 || b.size < size)
      continue;
    if (b.size > size)
      for (auto &tail : blocks)
        if (!tail.state) {
          tail = {b.offset + size, b.size - size, 1};
          b.size = size;
          break;
        }
    b.state = 2;
    live_bytes += b.size;
    peak_bytes = std::max(peak_bytes, live_bytes);
    return workspace_data + b.offset;
  }
  workspace_exhausted = true;
  return nullptr;
}
void bounded_free(void *p) {
  if (!p)
    return;
  for (auto &b : blocks)
    if (b.state == 2 && workspace_data + b.offset == p) {
      live_bytes -= b.size;
      b.state = 1;
      break;
    }
  bool merged;
  do {
    merged = false;
    for (auto &a : blocks)
      if (a.state == 1)
        for (auto &b : blocks)
          if (&a != &b && b.state == 1 && a.offset + a.size == b.offset) {
            a.size += b.size;
            b = {};
            merged = true;
          }
  } while (merged);
}
void *bounded_realloc(void *p, size_t size) {
  if (!p)
    return bounded_alloc(size);
  size_t old_size = 0;
  for (auto &b : blocks)
    if (b.state == 2 && workspace_data + b.offset == p)
      old_size = b.size;
  if (!old_size)
    return nullptr;
  if (size <= old_size)
    return p;
  void *next = bounded_alloc(size);
  if (next) {
    std::memcpy(next, p, old_size);
    bounded_free(p);
  }
  return next;
}
} // namespace
#define STBI_MALLOC(n) bounded_alloc(n)
#define STBI_REALLOC(p, n) bounded_realloc(p, n)
#define STBI_FREE(p) bounded_free(p)
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
// Allow metadata inspection of larger photos so the UI can report their actual
// size. read() rejects anything beyond the canvas limit BEFORE pixel decoding.
#define STBI_MAX_DIMENSIONS 32768
// The native clean-room CRT has no __assert ABI. Match stb's ordinary NDEBUG
// behavior locally instead of disabling assertions throughout the app. These
// are diagnostic invariants; the decoder's explicit malformed-input checks,
// dimension limits and bounded allocation failures still return errors.
#if defined(__PROSPERO__) || defined(WINE11_STBI_RELEASE_ASSERTS)
#define STBI_ASSERT(condition) ((void)0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
namespace ww::image {
namespace {
int last_error = 0;
void *mapped_workspace = nullptr;
size_t mapped_bytes = 0;
int normalized_error(int64_t result) {
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  uint32_t value = uint32_t(result);
  if ((value & 0xffff0000u) == 0x80020000u && (value & 0xffffu) &&
      (value & 0xffffu) < 4096)
    return int(value & 0xffffu);
  if (result < -1 && result > -4096)
    return int(-result);
#else
  (void)result;
#endif
  return errno > 0 ? errno : EIO;
}
bool failed(int64_t result) {
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  return result < 0 || (uint64_t(result) <= UINT32_MAX &&
                        (uint32_t(result) & 0xffff0000u) == 0x80020000u);
#else
  return result < 0;
#endif
}
int map_memory(size_t size, void *&memory) {
  memory = nullptr;
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int result = sceKernelMmap(nullptr, size, 3, 0x1002, -1, 0, &memory);
  if (result < 0)
    return -normalized_error(result);
#elif defined(_WIN32)
  memory =
      VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
  memory = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED)
    memory = nullptr;
#endif
  return memory ? 0 : -ENOMEM;
}
int unmap_memory(void *memory, size_t size) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int result = sceKernelMunmap(memory, size);
  return result < 0 ? -normalized_error(result) : 0;
#elif defined(_WIN32)
  (void)size;
  return VirtualFree(memory, 0, MEM_RELEASE) ? 0 : -EIO;
#else
  return ::munmap(memory, size) == 0 ? 0 : -normalized_error(-1);
#endif
}
int release_workspace() {
  if (mapped_workspace) {
    int result = unmap_memory(mapped_workspace, mapped_bytes);
    if (result < 0)
      return result;
    mapped_workspace = nullptr;
    mapped_bytes = 0;
  }
  workspace_data = workspace;
  workspace_capacity = sizeof(workspace);
  return 0;
}
int prepare_workspace(int width, int height) {
  if (width <= MaxWidth && height <= MaxHeight) {
    reset_workspace();
    return 0;
  }
  // Pixel decoding occurs only after header validation. The mapped workspace
  // is bounded and released before the fitted editing copy is published.
  size_t wanted = size_t(width) * height * 12 + WorkspaceBytes;
  wanted = (wanted + 0x3fff) & ~size_t(0x3fff);
  if (wanted > TemporaryWorkspaceLimit)
    return -EFBIG;
  void *memory = nullptr;
  int result = map_memory(wanted, memory);
  if (result < 0)
    return result;
  mapped_workspace = memory;
  mapped_bytes = wanted;
  workspace_data = static_cast<unsigned char *>(memory);
  workspace_capacity = wanted;
  reset_workspace();
  return 0;
}
bool valid_source_size(int width, int height) {
  return width > 0 && height > 0 && width <= MaxSourceDimension &&
         height <= MaxSourceDimension &&
         size_t(width) * height <= MaxSourcePixels;
}
void fit_copy(const unsigned char *decoded, int width, int height,
              int &out_width, int &out_height) {
  if (int64_t(width) * MaxHeight > int64_t(height) * MaxWidth) {
    out_width = MaxWidth;
    out_height = std::max(1, height * MaxWidth / width);
  } else {
    out_height = MaxHeight;
    out_width = std::max(1, width * MaxHeight / height);
  }
  // Bilinear sampling into the otherwise unused fixed workspace. The source
  // allocation stays intact until every fitted pixel is ready.
  for (int y = 0; y < out_height; ++y) {
    int sy = std::max(0, (2 * y + 1) * height * 128 / out_height - 128);
    int y0 = std::min(height - 1, sy / 256), y1 = std::min(height - 1, y0 + 1);
    unsigned fy = unsigned(sy & 255);
    for (int x = 0; x < out_width; ++x) {
      int sx = std::max(0, (2 * x + 1) * width * 128 / out_width - 128);
      int x0 = std::min(width - 1, sx / 256), x1 = std::min(width - 1, x0 + 1);
      unsigned fx = unsigned(sx & 255);
      uint32_t pixel = 0;
      for (int channel = 0; channel < 4; ++channel) {
        auto value = [&](int px, int py) {
          return decoded[(size_t(py) * width + px) * 4 + channel];
        };
        unsigned top = value(x0, y0) * (256 - fx) + value(x1, y0) * fx;
        unsigned bottom = value(x0, y1) * (256 - fx) + value(x1, y1) * fx;
        unsigned result = (top * (256 - fy) + bottom * fy + 32768) >> 16;
        pixel |= result << (channel * 8);
      }
      std::memcpy(workspace + (size_t(y) * out_width + x) * 4, &pixel, 4);
    }
  }
}
int file_open(const char *path, bool create) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  // Native flags use the payload SDK's FreeBSD ABI, also asserted in the
  // mocked native tests. Never truncate or open an existing copy for writing.
  int fd = ww::privilege::run(
      [&] { return sceKernelOpen(path, create ? 0x0001 | 0x0200 | 0x0800 : 0, 0666); });
#else
  int fd = ww::privilege::run([&] {
    return ::WW_OPEN(path,
                     create ? O_WRONLY | O_CREAT | O_EXCL | O_BINARY
                            : O_RDONLY | O_BINARY,
                     0666);
  });
#endif
  return fd < 0 ? -normalized_error(fd) : fd;
}
int64_t file_read(int fd, void *data, size_t size) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int64_t result = sceKernelRead(fd, data, size);
#else
  int64_t result = ::WW_READ(fd, data, unsigned(size));
#endif
  return failed(result)            ? -normalized_error(result)
         : uint64_t(result) > size ? -EIO
                                   : result;
}
int64_t file_write(int fd, const void *data, size_t size) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int64_t result = sceKernelWrite(fd, data, size);
#else
  int64_t result = ::WW_WRITE(fd, data, unsigned(size));
#endif
  return failed(result)            ? -normalized_error(result)
         : uint64_t(result) > size ? -EIO
                                   : result;
}
int file_seek(int fd, int64_t offset, int origin) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int64_t result = sceKernelLseek(fd, offset, origin);
#else
  int64_t result = ::WW_SEEK(fd, offset, origin);
#endif
  return failed(result) ? -normalized_error(result) : 0;
}
int file_sync(int fd) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int result = sceKernelFsync(fd);
#else
  int result = ::WW_SYNC(fd);
#endif
  return result < 0 ? -normalized_error(result) : 0;
}
int file_close(int fd) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int result = sceKernelClose(fd);
#else
  int result = ::WW_CLOSE(fd);
#endif
  return result < 0 ? -normalized_error(result) : 0;
}
int file_remove(const char *path) {
  errno = 0;
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_IMAGE_TEST)
  int result = sceKernelUnlink(path);
#else
  int result = ::WW_UNLINK(path);
#endif
  return result < 0 ? -normalized_error(result) : 0;
}
void fail(char *out, size_t capacity, const char *message) {
  if (capacity)
    std::snprintf(out, capacity, "%s", message);
}
void io_failure(char *out, size_t capacity, const char *stage, int code) {
  last_error = code > 0 ? code : EIO;
  const char *reason = last_error == EACCES || last_error == EPERM
                           ? "access denied"
                       : last_error == EROFS ? "read-only folder"
                       : last_error == ENOENT || last_error == ENODEV
                           ? "folder or drive unavailable"
                       : last_error == ENOSPC ? "drive is full"
                       : last_error == ENOMEM ? "not enough memory"
                       : last_error == EEXIST ? "copy name already exists"
                                              : "file I/O failed";
  std::snprintf(out, capacity, "%s: %s (error %d). Original unchanged.", stage,
                reason, last_error);
}
struct Input {
  int fd, error = 0;
  bool eof = false;
  int read(char *data, size_t size) {
    size_t done = 0;
    while (!error && !eof && done < size) {
      int64_t count = file_read(fd, data + done, size - done);
      if (count == -EINTR)
        continue;
      if (count < 0)
        error = int(-count);
      else if (!count)
        eof = true;
      else
        done += size_t(count);
    }
    return int(done);
  }
  bool seek(int64_t offset, int origin) {
    if (error)
      return false;
    int result = file_seek(fd, offset, origin);
    if (result < 0) {
      error = -result;
      return false;
    }
    eof = false;
    return true;
  }
};
int input_read(void *context, char *data, int bytes) {
  return bytes > 0 ? static_cast<Input *>(context)->read(data, size_t(bytes))
                   : 0;
}
void input_skip(void *context, int bytes) {
  static_cast<Input *>(context)->seek(bytes, SEEK_CUR);
}
int input_eof(void *context) {
  auto &file = *static_cast<Input *>(context);
  return file.eof || file.error;
}
uint32_t crc_update(uint32_t crc, const unsigned char *data, size_t n) {
  static constexpr auto table = [] {
    std::array<uint32_t, 256> values{};
    for (unsigned i = 0; i < values.size(); ++i) {
      uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit)
        value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
      values[i] = value;
    }
    return values;
  }();
  for (size_t i = 0; i < n; ++i) {
    crc = table[(crc ^ data[i]) & 255] ^ (crc >> 8);
  }
  return crc;
}
void big_endian(unsigned char *out, uint32_t n) {
  out[0] = static_cast<unsigned char>(n >> 24);
  out[1] = static_cast<unsigned char>(n >> 16);
  out[2] = static_cast<unsigned char>(n >> 8);
  out[3] = static_cast<unsigned char>(n);
}
struct Output {
  int fd;
  int error = 0;
  uint64_t hash = 14695981039346656037ull;
  size_t bytes = 0;
  void write(const void *source, size_t count) {
    if (error)
      return;
    auto *data = static_cast<const unsigned char *>(source);
    size_t done = 0;
    while (done < count) {
      auto n = file_write(fd, data + done, count - done);
      if (n == -EINTR)
        continue;
      if (n <= 0) {
        error = n < 0 ? int(-n) : EIO;
        return;
      }
      done += size_t(n);
    }
    bytes += count;
    for (size_t i = 0; i < count; ++i) {
      hash ^= data[i];
      hash *= 1099511628211ull;
    }
  }
  void number(uint32_t value) {
    unsigned char data[4];
    big_endian(data, value);
    write(data, sizeof(data));
  }
  void chunk(const char *name, const unsigned char *data, size_t n) {
    number(uint32_t(n));
    write(name, 4);
    write(data, n);
    auto crc = crc_update(0xffffffffu,
                          reinterpret_cast<const unsigned char *>(name), 4);
    number(crc_update(crc, data, n) ^ 0xffffffffu);
  }
};
} // namespace
size_t workspace_peak() { return peak_bytes; }
int error_code() { return last_error; }
int release_editable(EditingBuffer &buffer) {
  if (!buffer.pixels)
    return 0;
  int result = unmap_memory(buffer.pixels, buffer.bytes);
  if (!result)
    buffer = {};
  return result;
}
namespace {
EditingBuffer abandoned_buffer;
bool read_internal(const char *path, uint32_t *pixels, EditingBuffer *editing,
                   int &width, int &height, char *error, size_t capacity,
                   int *source_width, int *source_height) {
  last_error = 0;
  int reclaimed = release_editable(abandoned_buffer);
  if (reclaimed >= 0)
    reclaimed = release_workspace();
  if (reclaimed < 0) {
    io_failure(error, capacity, "Release image memory", -reclaimed);
    return false;
  }
  int fd =
      path && *path && (pixels || editing) && (!editing || !editing->pixels)
          ? file_open(path, false)
          : -EINVAL;
  if (fd < 0) {
    io_failure(error, capacity, "Open image", -fd);
    return false;
  }
  Input file{fd};
  reset_workspace();
  const stbi_io_callbacks callbacks{input_read, input_skip, input_eof};
  int w = 0, h = 0, components = 0;
  bool valid = stbi_info_from_callbacks(&callbacks, &file, &w, &h, &components);
  if (!valid || file.error || !valid_source_size(w, h)) {
    int closed = file_close(fd);
    if (file.error || closed < 0)
      io_failure(error, capacity, "Read image",
                 file.error ? file.error : -closed);
    else if (valid) {
      last_error = EFBIG;
      std::snprintf(error, capacity,
                    "Image is %d x %d. Supported photos are up to 4K (4096 x "
                    "2160 pixels, including portrait).",
                    w, h);
    } else {
      last_error = EILSEQ;
      fail(
          error, capacity,
          "This file is damaged or is not a supported PNG, JPEG or BMP image.");
    }
    return false;
  }
  if (!file.seek(0, SEEK_SET)) {
    file_close(fd);
    io_failure(error, capacity, "Seek image", file.error);
    return false;
  }
  int prepared = prepare_workspace(w, h);
  if (prepared < 0) {
    file_close(fd);
    io_failure(error, capacity, "Allocate image memory", -prepared);
    return false;
  }
  auto *decoded =
      stbi_load_from_callbacks(&callbacks, &file, &w, &h, &components, 4);
  int closed = file_close(fd);
  bool ok = decoded && !file.error && closed >= 0 && valid_source_size(w, h);
  if (!ok) {
    if (file.error || closed < 0)
      io_failure(error, capacity, "Read image",
                 file.error ? file.error : -closed);
    else {
      last_error = workspace_exhausted ? ENOMEM : EILSEQ;
      fail(error, capacity,
           workspace_exhausted ? "Not enough decode memory for this image. "
                                 "Your previous image is unchanged."
                               : "The image is damaged or unsupported. Your "
                                 "previous image is unchanged.");
    }
    stbi_image_free(decoded);
    release_workspace();
    return false;
  }
  int out_w = w, out_h = h;
  EditingBuffer candidate;
  if (editing) {
    size_t bytes = (size_t(w) * h * 8 + 0x3fff) & ~size_t(0x3fff);
    void *memory = nullptr;
    int allocated = map_memory(bytes, memory);
    if (allocated < 0) {
      stbi_image_free(decoded);
      release_workspace();
      io_failure(error, capacity, "Allocate editing canvas", -allocated);
      return false;
    }
    candidate = {static_cast<uint32_t *>(memory),
                 static_cast<uint32_t *>(memory) + size_t(w) * h, bytes};
    std::memcpy(candidate.pixels, decoded, size_t(w) * h * 4);
  } else if (mapped_workspace) {
    if (w > MaxWidth || h > MaxHeight)
      fit_copy(decoded, w, h, out_w, out_h);
    else
      std::memcpy(workspace, decoded, size_t(w) * h * 4);
  } else if (w > MaxWidth || h > MaxHeight) {
    stbi_image_free(decoded);
    last_error = EAGAIN;
    fail(error, capacity,
         "Image changed while opening. Please try again; previous image "
         "unchanged.");
    return false;
  } else
    std::memcpy(pixels, decoded, size_t(w) * h * 4);
  stbi_image_free(decoded);
  bool was_mapped = mapped_workspace != nullptr;
  int released = release_workspace();
  if (released < 0) {
    if (candidate.pixels && release_editable(candidate) < 0)
      abandoned_buffer = candidate;
    io_failure(error, capacity, "Release decode memory", -released);
    return false;
  }
  if (editing)
    *editing = candidate;
  else if (was_mapped)
    std::memcpy(pixels, workspace, size_t(out_w) * out_h * 4);
  width = out_w;
  height = out_h;
  if (source_width)
    *source_width = w;
  if (source_height)
    *source_height = h;
  fail(error, capacity, "");
  return true;
}
} // namespace
bool read(const char *path, uint32_t *pixels, int &width, int &height,
          char *error, size_t capacity, int *source_width, int *source_height) {
  return read_internal(path, pixels, nullptr, width, height, error, capacity,
                       source_width, source_height);
}
bool read_memory(const unsigned char* bytes, size_t length, uint32_t* pixels, int& width, int& height) {
  if (!bytes || !pixels || !length || length > 2 * 1024 * 1024) return false;
  if (release_workspace() < 0) return false;
  reset_workspace();
  int w = 0, h = 0, components = 0;
  if (!stbi_info_from_memory(bytes, int(length), &w, &h, &components) || !valid_source_size(w, h) ||
      prepare_workspace(w, h) < 0) return false;
  auto* decoded = stbi_load_from_memory(bytes, int(length), &w, &h, &components, 4);
  if (!decoded || !valid_source_size(w, h)) {
    stbi_image_free(decoded); release_workspace(); return false;
  }
  int out_w = w, out_h = h;
  const bool mapped = mapped_workspace != nullptr;
  if (mapped) {
    if (w > MaxWidth || h > MaxHeight) fit_copy(decoded, w, h, out_w, out_h);
    else std::memcpy(workspace, decoded, size_t(w) * h * 4);
  } else if (w <= MaxWidth && h <= MaxHeight) {
    std::memcpy(pixels, decoded, size_t(w) * h * 4);
  } else {
    stbi_image_free(decoded); release_workspace(); return false;
  }
  stbi_image_free(decoded);
  if (release_workspace() < 0) return false;
  if (mapped) std::memcpy(pixels, workspace, size_t(out_w) * out_h * 4);
  width = out_w; height = out_h; return true;
}
bool read_editable(const char *path, EditingBuffer &replacement, int &width,
                   int &height, char *error, size_t capacity) {
  return read_internal(path, nullptr, &replacement, width, height, error,
                       capacity, nullptr, nullptr);
}
namespace {
alignas(16) unsigned char save_buffer[64 * 1024];
PngSaveJob *save_owner = nullptr;
uint64_t save_clock_us() {
#ifdef _WIN32
  LARGE_INTEGER frequency{}, now{};
  if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&now))
    return 0;
  uint64_t ticks = uint64_t(now.QuadPart), hz = uint64_t(frequency.QuadPart);
  return (ticks / hz) * 1000000 + (ticks % hz) * 1000000 / hz;
#else
  timespec now{};
  return ::clock_gettime(CLOCK_MONOTONIC, &now) == 0
             ? uint64_t(now.tv_sec) * 1000000 + uint64_t(now.tv_nsec) / 1000
             : 0;
#endif
}
void save_failure(PngSaveJob &job, const char *stage, int code) {
  job.error = code > 0 ? code : EIO;
  std::snprintf(job.failure_stage, sizeof(job.failure_stage), "%s", stage);
  job.phase = SavePhase::cleanup;
}
void append_save(PngSaveJob &job, const void *source, size_t count) {
  if (count > sizeof(save_buffer) - job.queued) {
    save_failure(job, "Encode PNG copy", EOVERFLOW);
    return;
  }
  const auto *data = static_cast<const unsigned char *>(source);
  if (count)
    std::memcpy(save_buffer + job.queued, data, count);
  job.queued += count;
  for (size_t i = 0; i < count; ++i) {
    job.hash ^= data[i];
    job.hash *= 1099511628211ull;
  }
}
void append_number(PngSaveJob &job, uint32_t value) {
  unsigned char data[4];
  big_endian(data, value);
  append_save(job, data, 4);
}
void append_chunk(PngSaveJob &job, const char *name, const unsigned char *data,
                  size_t count) {
  append_number(job, uint32_t(count));
  append_save(job, name, 4);
  append_save(job, data, count);
  uint32_t crc =
      crc_update(0xffffffffu, reinterpret_cast<const unsigned char *>(name), 4);
  append_number(job, crc_update(crc, data, count) ^ 0xffffffffu);
}
void append_payload(PngSaveJob &job, const unsigned char *data, size_t count) {
  append_save(job, data, count);
  job.crc = crc_update(job.crc, data, count);
}
} // namespace
int allocate_history(HistoryBuffer &buffer) {
  if (buffer.data)
    return 0;
  void *memory = nullptr;
  int result = map_memory(HistoryBytes, memory);
  if (!result)
    buffer = {static_cast<unsigned char *>(memory), HistoryBytes};
  return result;
}
int release_history(HistoryBuffer &buffer) {
  if (!buffer.data)
    return 0;
  int result = unmap_memory(buffer.data, buffer.bytes);
  if (!result)
    buffer = {};
  return result;
}
bool PngSaveJob::begin(const char *target, const uint32_t *source, int w,
                       int h) {
  if (busy() || save_owner)
    return false;
  char copy[sizeof(path)];
  if (!target || std::strlen(target) >= sizeof(path) || !source ||
      !valid_source_size(w, h)) {
    phase = SavePhase::failed;
    error = EINVAL;
    io_failure(message, sizeof(message), "Start PNG copy", error);
    return false;
  }
  std::snprintf(copy, sizeof(copy), "%s", target);
  *this = PngSaveJob{};
  std::memcpy(path, copy, std::strlen(copy) + 1);
  width = w;
  height = h;
  pixels = source;
  total = 63 + size_t(height) * (6 + size_t(width) * 4);
  phase = SavePhase::create;
  save_owner = this;
  return true;
}
const char *PngSaveJob::stage() const {
  switch (phase) {
  case SavePhase::create:
    return "Creating PNG copy";
  case SavePhase::header:
  case SavePhase::rows:
  case SavePhase::trailer:
    return "Writing PNG copy";
  case SavePhase::flush:
    return "Flushing the drive";
  case SavePhase::close:
  case SavePhase::reopen:
    return "Preparing verification";
  case SavePhase::verify:
  case SavePhase::verify_close:
    return "Verifying saved copy";
  case SavePhase::cleanup:
    return cancel_requested ? "Canceling save" : "Removing incomplete copy";
  case SavePhase::done:
    return "Copy saved";
  case SavePhase::failed:
    return "Copy not saved";
  case SavePhase::canceled:
    return "Save canceled";
  default:
    return "Ready";
  }
}
unsigned PngSaveJob::progress() const {
  if (phase == SavePhase::done)
    return 100;
  if (!total)
    return 0;
  if (phase >= SavePhase::verify && phase <= SavePhase::verify_close)
    return unsigned(std::min(size_t(99), 72 + verified * 27 / total));
  return unsigned(std::min(size_t(70), written * 70 / total));
}
void PngSaveJob::step(size_t byte_budget) {
  if (!busy() || save_owner != this)
    return;
  if (cancel_requested && phase != SavePhase::cleanup)
    phase = SavePhase::cleanup;
  if (phase == SavePhase::cleanup) {
    if (fd >= 0) {
      int result = file_close(fd);
      fd = -1;
      if (result < 0 && !error) {
        error = -result;
        std::snprintf(failure_stage, sizeof(failure_stage), "Close PNG copy");
      }
    }
    int removed = created ? file_remove(path) : 0;
    if (error)
      io_failure(message, sizeof(message), failure_stage, error);
    else
      std::snprintf(message, sizeof(message),
                    "Save canceled. Original and drawing are unchanged.");
    if (removed < 0) {
      error = -removed;
      std::snprintf(message, sizeof(message),
                    "Incomplete copy could not be removed (error %d). Original "
                    "and drawing unchanged.",
                    error);
    }
    phase = error ? SavePhase::failed : SavePhase::canceled;
    save_owner = nullptr;
    return;
  }
  if (phase == SavePhase::create) {
    fd = file_open(path, true);
    if (fd < 0) {
      save_failure(*this, "Create PNG copy", -fd);
      fd = -1;
      return;
    }
    created = true;
    phase = SavePhase::header;
    return;
  }
  if (phase == SavePhase::flush && sent == queued) {
    int result = file_sync(fd);
    if (result < 0)
      save_failure(*this, "Flush PNG copy", -result);
    else
      phase = SavePhase::close;
    return;
  }
  if (phase == SavePhase::close) {
    int result = file_close(fd);
    fd = -1;
    if (result < 0)
      save_failure(*this, "Close PNG copy", -result);
    else
      phase = SavePhase::reopen;
    return;
  }
  if (phase == SavePhase::reopen) {
    fd = file_open(path, false);
    if (fd < 0) {
      save_failure(*this, "Reopen PNG copy", -fd);
      fd = -1;
    } else
      phase = SavePhase::verify;
    return;
  }
  if (phase == SavePhase::verify_close) {
    int result = file_close(fd);
    fd = -1;
    if (result < 0)
      save_failure(*this, "Verify PNG copy", -result);
    else {
      phase = SavePhase::done;
      save_owner = nullptr;
      message[0] = 0;
    }
    return;
  }
  byte_budget = std::min(byte_budget, size_t(512 * 1024));
  if (!byte_budget)
    return;
  const uint64_t started = save_clock_us();
  auto expired = [&] { return started && save_clock_us() - started >= 3000; };
  if (phase == SavePhase::verify) {
    unsigned calls = 0;
    while (byte_budget && calls++ < 32) {
      int64_t result = file_read(fd, save_buffer,
                                 std::min(byte_budget, sizeof(save_buffer)));
      if (result == -EINTR)
        return;
      if (result < 0) {
        save_failure(*this, "Read saved copy", int(-result));
        return;
      }
      if (!result) {
        if (verified != total || verify_hash != hash)
          save_failure(*this, "Verify PNG copy", EIO);
        else
          phase = SavePhase::verify_close;
        return;
      }
      verified += size_t(result);
      if (verified > total) {
        save_failure(*this, "Verify PNG copy", EIO);
        return;
      }
      for (int64_t i = 0; i < result; ++i) {
        verify_hash ^= save_buffer[i];
        verify_hash *= 1099511628211ull;
      }
      byte_budget -= size_t(result);
      if (expired())
        return;
    }
    return;
  }
  unsigned calls = 0;
  while (busy() && byte_budget && calls < 32) {
    if (sent < queued) {
      size_t request = std::min(byte_budget, queued - sent);
      int64_t result = file_write(fd, save_buffer + sent, request);
      ++calls;
      if (result == -EINTR)
        return;
      if (result <= 0) {
        save_failure(*this, "Write PNG copy", result < 0 ? int(-result) : EIO);
        return;
      }
      sent += size_t(result);
      written += size_t(result);
      byte_budget -= size_t(result);
      if (sent < queued)
        return;
      sent = queued = 0;
      if (phase == SavePhase::flush)
        return; // Render finishing stage before fsync.
      if (expired())
        return;
    }
    if (phase == SavePhase::header) {
      const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
      append_save(*this, signature, sizeof(signature));
      unsigned char ihdr[13]{};
      big_endian(ihdr, uint32_t(width));
      big_endian(ihdr + 4, uint32_t(height));
      ihdr[8] = 8;
      ihdr[9] = 6;
      append_chunk(*this, "IHDR", ihdr, sizeof(ihdr));
      append_number(*this,
                    uint32_t(2 + size_t(height) * (6 + size_t(width) * 4) + 4));
      append_save(*this, "IDAT", 4);
      const unsigned char zlib_header[] = {0x78, 0x01};
      append_payload(*this, zlib_header, 2);
      phase = SavePhase::rows;
    } else if (phase == SavePhase::rows) {
      if (row == height) {
        phase = SavePhase::trailer;
        continue;
      }
      // Combine scanlines into one bounded USB write instead of issuing tiny
      // block/filter/data writes for every row.
      while (row < height &&
             queued + 6 + size_t(width) * 4 <= sizeof(save_buffer)) {
        uint16_t len = uint16_t(size_t(width) * 4 + 1),
                 inverse = uint16_t(~len);
        const unsigned char block[] = {
            static_cast<unsigned char>(row == height - 1),
            static_cast<unsigned char>(len),
            static_cast<unsigned char>(len >> 8),
            static_cast<unsigned char>(inverse),
            static_cast<unsigned char>(inverse >> 8)};
        append_payload(*this, block, sizeof(block));
        const unsigned char filter = 0;
        append_payload(*this, &filter, 1);
        const auto *data = reinterpret_cast<const unsigned char *>(
            pixels + size_t(row) * width);
        append_payload(*this, data, size_t(width) * 4);
        adler_b = (adler_b + adler_a) % 65521; // Filter byte is zero.
        for (size_t i = 0; i < size_t(width) * 4; ++i) {
          adler_a = (adler_a + data[i]) % 65521;
          adler_b = (adler_b + adler_a) % 65521;
        }
        ++row;
      }
    } else if (phase == SavePhase::trailer) {
      unsigned char adler[4];
      big_endian(adler, adler_b << 16 | adler_a);
      append_payload(*this, adler, 4);
      append_number(*this, crc ^ 0xffffffffu);
      append_chunk(*this, "IEND", nullptr, 0);
      phase = SavePhase::flush;
    } else
      return;
  }
}
bool write_new_png(const char *path, const uint32_t *pixels, int width,
                   int height, char *error, size_t capacity) {
  PngSaveJob job;
  if (!job.begin(path, pixels, width, height)) {
    last_error = job.error ? job.error : EBUSY;
    if (job.message[0])
      fail(error, capacity, job.message);
    else
      io_failure(error, capacity, "Start PNG copy", last_error);
    return false;
  }
  while (job.busy())
    job.step();
  last_error = job.error;
  fail(error, capacity, job.message);
  return job.phase == SavePhase::done;
}
} // namespace ww::image
