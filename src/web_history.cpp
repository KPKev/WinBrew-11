// SPDX-License-Identifier: GPL-3.0-or-later
#include "web_history.hpp"
#include "explorer_directory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#if defined(__PROSPERO__) || defined(WINE11_NATIVE_HISTORY_TEST)
#define WW_HISTORY_KERNEL_IO 1
extern "C" {
int sceKernelOpen(const char *, int, int);
int64_t sceKernelRead(int, void *, size_t);
int64_t sceKernelWrite(int, const void *, size_t);
int sceKernelFsync(int);
int sceKernelClose(int);
int sceKernelRename(const char *, const char *);
int sceKernelUnlink(const char *);
}
#endif
namespace ww {
namespace {
constexpr char Header[] = "WINE11_RECENT_SITES 1\n";
constexpr size_t DataBytes = sizeof(Header) + WebHistory::Limit * WebHistory::OriginBytes;
bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; }
int normalize(int value) {
#ifdef WW_HISTORY_KERNEL_IO
  return value < 0 ? -directory::native_error(value) : value;
#else
  return value < 0 ? -directory::error_or_io(errno) : value;
#endif
}
int file_open(const char *name, bool writing) {
#ifdef WW_HISTORY_KERNEL_IO
  return normalize(sceKernelOpen(name, writing ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, 0600));
#elif defined(_WIN32)
  return normalize(_open(name, (writing ? _O_WRONLY | _O_CREAT | _O_EXCL : _O_RDONLY) | _O_BINARY, _S_IREAD | _S_IWRITE));
#else
  return normalize(::open(name, writing ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, 0600));
#endif
}
int64_t file_read(int fd, void *data, size_t size) {
#ifdef WW_HISTORY_KERNEL_IO
  const auto result = sceKernelRead(fd, data, size);
#elif defined(_WIN32)
  const auto result = _read(fd, data, unsigned(size));
#else
  const auto result = ::read(fd, data, size);
#endif
  return result < 0 ? normalize(int(result)) : result;
}
int64_t file_write(int fd, const void *data, size_t size) {
#ifdef WW_HISTORY_KERNEL_IO
  const auto result = sceKernelWrite(fd, data, size);
#elif defined(_WIN32)
  const auto result = _write(fd, data, unsigned(size));
#else
  const auto result = ::write(fd, data, size);
#endif
  return result < 0 ? normalize(int(result)) : result;
}
int file_sync(int fd) {
#ifdef WW_HISTORY_KERNEL_IO
  return normalize(sceKernelFsync(fd));
#elif defined(_WIN32)
  return normalize(_commit(fd));
#else
  return normalize(::fsync(fd));
#endif
}
int file_close(int fd) {
#ifdef WW_HISTORY_KERNEL_IO
  return normalize(sceKernelClose(fd));
#elif defined(_WIN32)
  return normalize(_close(fd));
#else
  return normalize(::close(fd));
#endif
}
void file_remove(const char *name) {
#ifdef WW_HISTORY_KERNEL_IO
  (void)sceKernelUnlink(name);
#else
  (void)std::remove(name);
#endif
}
int file_replace(const char *from, const char *to) {
#ifdef WW_HISTORY_KERNEL_IO
  return normalize(sceKernelRename(from, to));
#elif defined(_WIN32)
  return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -EIO;
#else
  return normalize(std::rename(from, to));
#endif
}
// Bounded read, including one extra byte to detect truncation. Native reads
// use kernel imports, not compatibility-CRT descriptor functions.
bool read_file(const char *name, char *data, size_t capacity, size_t &size, int &error) {
  const int fd = file_open(name, false);
  if (fd < 0) { error = -fd; return false; }
  size = 0;
  while (size < capacity) {
    const auto count = file_read(fd, data + size, capacity - size);
    if (count < 0) { error = int(-count); file_close(fd); return false; }
    if (!count) break;
    if (size_t(count) > capacity - size) { error = EIO; file_close(fd); return false; }
    size += size_t(count);
  }
  const int closed = file_close(fd);
  error = closed < 0 ? -closed : 0;
  return closed == 0;
}
}
bool WebHistory::origin(const char *url, char *out, size_t capacity) {
  if (!url || !out || !capacity) return false;
  const bool secure = !std::strncmp(url, "https://", 8);
  const size_t scheme = secure ? 8 : 7;
  if (!secure && std::strncmp(url, "http://", 7)) return false;
  const char *host = url + scheme, *end = host;
  while (*end && *end != '/' && *end != '?' && *end != '#') ++end;
  if (host == end || size_t(end - url) >= capacity) return false;
  for (const char *p = url; *p; ++p)
    if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 || *p == '\\') return false;
  const char *port = nullptr, *host_end = end;
  if (*host == '[') {
    const char *bracket = host + 1;
    bool colon = false;
    for (; bracket < end && *bracket != ']'; ++bracket) {
      const char c = lower(*bracket);
      if (c == ':') colon = true;
      else if (!(digit(c) || (c >= 'a' && c <= 'f') || c == '.')) return false;
    }
    if (bracket == end || !colon) return false;
    host_end = bracket + 1;
    if (host_end < end) {
      if (*host_end != ':') return false;
      port = host_end + 1;
    }
  } else {
    for (const char *p = host; p < end; ++p) {
      if (*p == ':') { host_end = p; port = p + 1; break; }
      if (!(alpha(*p) || digit(*p) || *p == '.' || *p == '-')) return false;
    }
    if (host_end > host && host_end[-1] == '.') --host_end;
    if (host_end == host || *host == '.' || host_end[-1] == '.') return false;
    bool label_start = true;
    for (const char *p = host; p < host_end; ++p) {
      if (*p == '.') {
        if (label_start || p[-1] == '-') return false;
        label_start = true;
      } else {
        if (label_start && *p == '-') return false;
        label_start = false;
      }
    }
    if (label_start || host_end[-1] == '-') return false;
  }
  unsigned number = 0;
  if (port) {
    if (port == end) return false;
    for (const char *p = port; p < end; ++p) {
      if (!digit(*p)) return false;
      number = number * 10 + unsigned(*p - '0');
      if (number > 65535) return false;
    }
    if (!number) return false;
    if (number == (secure ? 443u : 80u)) port = nullptr;
  }
  size_t n = scheme + size_t(host_end - host);
  if (n + 1 > capacity) return false;
  std::memcpy(out, secure ? "https://" : "http://", scheme);
  for (size_t i = scheme; i < n; ++i) out[i] = lower(host[i - scheme]);
  out[n] = 0;
  if (port) {
    const int written = std::snprintf(out + n, capacity - n, ":%u", number);
    if (written < 0 || size_t(written) >= capacity - n) { out[0] = 0; return false; }
  }
  return true;
}
bool WebHistory::load(const char *storage_path) {
  if (!storage_path || !*storage_path || std::strlen(storage_path) >= sizeof(path)) {
    error = ENAMETOOLONG; return false;
  }
  char copy[sizeof(path)]{};
  std::snprintf(copy, sizeof(copy), "%s", storage_path);
  std::memcpy(path, copy, sizeof(path));
  char data[DataBytes]{};
  size_t size = 0;
  if (!read_file(path, data, sizeof(data), size, error)) {
    if (error == ENOENT) { count = 0; dirty = false; error = 0; std::memset(sites, 0, sizeof(sites)); return true; }
    return false;
  }
  if (size >= sizeof(data) || size < sizeof(Header) - 1 ||
      std::memcmp(data, Header, sizeof(Header) - 1) ||
      std::memchr(data, 0, size)) { error = EINVAL; return false; }
  char loaded[Limit][OriginBytes]{};
  int found = 0;
  size_t offset = sizeof(Header) - 1;
  while (offset < size) {
    const auto *newline = static_cast<const char *>(std::memchr(data + offset, '\n', size - offset));
    if (!newline || found == Limit) { error = EINVAL; return false; }
    const size_t length = size_t(newline - data) - offset;
    if (!length || length >= OriginBytes) { error = EINVAL; return false; }
    char candidate[OriginBytes]{};
    std::memcpy(candidate, data + offset, length);
    if (!origin(candidate, loaded[found], OriginBytes) || std::strcmp(candidate, loaded[found])) {
      error = EINVAL; return false;
    }
    for (int i = 0; i < found; ++i)
      if (!std::strcmp(loaded[i], candidate)) { error = EINVAL; return false; }
    ++found;
    offset += length + 1;
  }
  std::memcpy(sites, loaded, sizeof(sites)); count = found; error = 0; dirty = false; return true;
}
bool WebHistory::remember(const char *url) {
  char site[OriginBytes]{};
  if (!origin(url, site, sizeof(site))) { error = EINVAL; return false; }
  int position = count;
  for (int i = 0; i < count; ++i)
    if (!std::strcmp(sites[i], site)) { position = i; break; }
  if (count && position == 0) { if (dirty) return save(); error = 0; return true; }
  if (position >= Limit) position = Limit - 1;
  for (int i = position; i > 0; --i) std::memcpy(sites[i], sites[i - 1], OriginBytes);
  std::memcpy(sites[0], site, OriginBytes);
  if (count < Limit && position == count) ++count;
  dirty = true;
  return save();
}
bool WebHistory::clear() {
  count = 0;
  std::memset(sites, 0, sizeof(sites));
  dirty = true;
  return save();
}
bool WebHistory::save() {
  if (!path[0]) { error = ENOENT; return false; }
  char data[DataBytes]{};
  size_t size = sizeof(Header) - 1;
  std::memcpy(data, Header, size);
  for (int i = 0; i < count; ++i) {
    const size_t length = std::strlen(sites[i]);
    if (length >= OriginBytes || size + length + 1 >= sizeof(data)) { error = EINVAL; return false; }
    std::memcpy(data + size, sites[i], length); size += length; data[size++] = '\n';
  }
  static unsigned serial = 0;
  char temporary[1080]{};
  int fd = -EEXIST;
  for (int attempt = 0; attempt < 16 && fd == -EEXIST; ++attempt) {
    const int length = std::snprintf(temporary, sizeof(temporary), "%s.wine11-recent-%u.tmp", path, ++serial);
    if (length < 0 || size_t(length) >= sizeof(temporary)) { error = ENAMETOOLONG; return false; }
    fd = file_open(temporary, true);
  }
  if (fd < 0) { error = -fd; return false; }
  auto fail = [&](int code) { file_remove(temporary); error = code ? code : EIO; return false; };
  size_t offset = 0;
  int failure = 0;
  while (offset < size) {
    const auto written = file_write(fd, data + offset, size - offset);
    if (written < 0) { failure = int(-written); break; }
    if (!written || size_t(written) > size - offset) { failure = EIO; break; }
    offset += size_t(written);
  }
  if (!failure) { const int synced = file_sync(fd); if (synced < 0) failure = -synced; }
  const int closed = file_close(fd);
  if (closed < 0 && !failure) failure = -closed;
  if (failure) return fail(failure);
  char actual[DataBytes]{};
  size_t got = 0;
  int checked_error = 0;
  if (!read_file(temporary, actual, sizeof(actual), got, checked_error)) return fail(checked_error);
  if (got != size || std::memcmp(data, actual, size)) return fail(EIO);
  const int replaced = file_replace(temporary, path);
  if (replaced < 0) return fail(-replaced);
  error = 0;
  dirty = false;
  return true;
}
} // namespace ww
