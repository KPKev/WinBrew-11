// SPDX-License-Identifier: GPL-3.0-or-later
#include "explorer_fileops.hpp"
#include "privilege.hpp"
#include "explorer_directory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __PROSPERO__
extern "C" {
int sceKernelOpen(const char *, int, int);
int64_t sceKernelRead(int, void *, size_t);
int64_t sceKernelWrite(int, const void *, size_t);
int64_t sceKernelLseek(int, int64_t, int);
int sceKernelFstat(int, struct stat *);
int sceKernelFchmod(int, unsigned);
int sceKernelFsync(int);
int sceKernelClose(int);
}
#endif
namespace ww::fileops {
namespace {
// MSVC has no ESTALE. A changed file/path is retryable there; native POSIX
// builds retain their platform's exact stale-handle error value.
#ifdef ESTALE
constexpr int ChangedPath = ESTALE;
#else
constexpr int ChangedPath = EAGAIN;
#endif
int error_code(int code) {
#ifdef __PROSPERO__
  return code < 0 ? -directory::native_error(code) : code;
#else
  return code < 0 ? -directory::error_or_io(errno) : code;
#endif
}
int open_file(void *, const char *path, bool create) {
#ifdef _WIN32
  HANDLE handle = CreateFileA(
      path, GENERIC_READ | (create ? GENERIC_WRITE : 0),
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      create ? CREATE_NEW : OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    const auto e = GetLastError();
    return e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS      ? -EEXIST
           : e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? -ENOENT
                                                                    : -EACCES;
  }
  BY_HANDLE_FILE_INFORMATION info{};
  if (!GetFileInformationByHandle(handle, &info) ||
      (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
      GetFileType(handle) != FILE_TYPE_DISK) {
    CloseHandle(handle);
    return -ELOOP;
  }
  const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                 _O_BINARY | (create ? _O_RDWR : _O_RDONLY));
  if (fd < 0)
    CloseHandle(handle);
  return error_code(fd);
#else
  const int flags =
      O_NOFOLLOW | O_NONBLOCK | (create ? O_RDWR | O_CREAT | O_EXCL : O_RDONLY);
#ifdef __PROSPERO__
  return error_code(ww::privilege::run([&] { return sceKernelOpen(path, flags, 0600); }));
#else
  return error_code(ww::privilege::run([&] { return ::open(path, flags, 0600); }));
#endif
#endif
}
int64_t read_file(void *, int fd, void *buffer, size_t count) {
#ifdef __PROSPERO__
  const auto got = sceKernelRead(fd, buffer, count);
#elif defined(_WIN32)
  const auto got = _read(fd, buffer, unsigned(count));
#else
  const auto got = ::read(fd, buffer, count);
#endif
  return got < 0 ? error_code(int(got)) : got;
}
int64_t write_file(void *, int fd, const void *buffer, size_t count) {
#ifdef __PROSPERO__
  const auto got = sceKernelWrite(fd, buffer, count);
#elif defined(_WIN32)
  const auto got = _write(fd, buffer, unsigned(count));
#else
  const auto got = ::write(fd, buffer, count);
#endif
  return got < 0 ? error_code(int(got)) : got;
}
int rewind_file(void *, int fd) {
#ifdef __PROSPERO__
  const auto position = sceKernelLseek(fd, 0, SEEK_SET);
#elif defined(_WIN32)
  const auto position = _lseeki64(fd, 0, SEEK_SET);
#else
  const auto position = ::lseek(fd, 0, SEEK_SET);
#endif
  return position < 0 ? error_code(int(position)) : position == 0 ? 0 : -EIO;
}
int metadata(void *, int fd, Info &out) {
#ifdef _WIN32
  BY_HANDLE_FILE_INFORMATION value{};
  if (!GetFileInformationByHandle(reinterpret_cast<HANDLE>(_get_osfhandle(fd)),
                                  &value))
    return -EIO;
  if (value.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    return -ELOOP;
  out.device = value.dwVolumeSerialNumber;
  out.inode = uint64_t(value.nFileIndexHigh) << 32 | value.nFileIndexLow;
  out.size = uint64_t(value.nFileSizeHigh) << 32 | value.nFileSizeLow;
  const uint64_t time = uint64_t(value.ftLastWriteTime.dwHighDateTime) << 32 |
                        value.ftLastWriteTime.dwLowDateTime;
  out.modified = int64_t(time / 10000000) - 11644473600ll;
  out.modified_ns = long((time % 10000000) * 100);
  out.changed = out.modified;
  out.changed_ns = out.modified_ns;
  out.directory = (value.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  out.regular = !out.directory;
  out.mode = (value.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
  return 0;
#else
  struct stat value{};
#ifdef __PROSPERO__
  const int code = error_code(sceKernelFstat(fd, &value));
#else
  const int code = error_code(::fstat(fd, &value));
#endif
  if (code < 0)
    return code;
  if (value.st_size < 0)
    return -EIO;
  out.device = value.st_dev;
  out.inode = value.st_ino;
  out.size = uint64_t(value.st_size);
  out.modified = value.st_mtime;
  out.modified_ns = value.st_mtim.tv_nsec;
  out.changed = value.st_ctime;
  out.changed_ns = value.st_ctim.tv_nsec;
  out.mode = unsigned(value.st_mode & 0777);
  out.regular = S_ISREG(value.st_mode);
  out.directory = S_ISDIR(value.st_mode);
  return 0;
#endif
}
int permissions(void *, int fd, unsigned mode) {
#ifdef __PROSPERO__
  return error_code(ww::privilege::run([&] { return sceKernelFchmod(fd, mode); }));
#elif defined(_WIN32)
  (void)fd;
  (void)mode;
  return 0;
#else
  return error_code(ww::privilege::run([&] { return ::fchmod(fd, mode); }));
#endif
}
int sync_file(void *, int fd) {
#ifdef __PROSPERO__
  return error_code(sceKernelFsync(fd));
#elif defined(_WIN32)
  return error_code(_commit(fd));
#else
  return error_code(::fsync(fd));
#endif
}
int close_file(void *, int fd) {
#ifdef __PROSPERO__
  return error_code(sceKernelClose(fd));
#elif defined(_WIN32)
  return error_code(_close(fd));
#else
  return error_code(::close(fd));
#endif
}
const Io defaults{nullptr,  open_file,   read_file, write_file, rewind_file,
                  metadata, permissions, sync_file, close_file};
bool usable(const Io &io) {
  return io.open && io.read && io.write && io.rewind && io.info &&
         io.permissions && io.sync && io.close;
}
const char *leaf(const char *path) {
  const char *slash = std::strrchr(path, '/');
#ifdef _WIN32
  const char *back = std::strrchr(path, '\\');
  if (back && (!slash || back > slash))
    slash = back;
#endif
  return slash ? slash + 1 : path;
}
bool destination_name(char *out, size_t capacity, const char *folder,
                      const char *name, int number) {
  char candidate[256];
  if (!number)
    std::snprintf(candidate, sizeof(candidate), "%.255s", name);
  else {
    const char *dot = std::strrchr(name, '.');
    if (dot == name || (dot && std::strlen(dot) > 20))
      dot = nullptr;
    const size_t extension = dot ? std::strlen(dot) : 0;
    char suffix[40];
    if (number == 1)
      std::snprintf(suffix, sizeof(suffix), " - Copy");
    else
      std::snprintf(suffix, sizeof(suffix), " - Copy (%d)", number);
    const size_t max_stem = 255 - extension - std::strlen(suffix);
    size_t stem = dot ? size_t(dot - name) : std::strlen(name);
    if (stem > max_stem)
      stem = max_stem;
    std::snprintf(candidate, sizeof(candidate), "%.*s%s%s", int(stem), name,
                  suffix, dot ? dot : "");
  }
  const size_t size = std::strlen(folder);
  const int count = std::snprintf(
      out, capacity, "%s%s%s", folder,
      size && (folder[size - 1] == '/' || folder[size - 1] == '\\') ? "" : "/",
      candidate);
  return count > 0 && size_t(count) < capacity;
}
int read_exact(const Io &io, int fd, unsigned char *out, size_t count) {
  size_t offset = 0;
  while (offset < count) {
    const auto got = io.read(io.context, fd, out + offset, count - offset);
    if (got < 0)
      return int(got);
    if (!got || uint64_t(got) > count - offset)
      return -EIO;
    offset += size_t(got);
  }
  return 0;
}
} // namespace
const Io &system_io() { return defaults; }
int inspect(const char *path, Info &info, const Io *provided) {
  if (!path || !*path || std::strlen(path) >= 1024)
    return -EINVAL;
  const auto &io = provided ? *provided : defaults;
  if (!usable(io))
    return -EINVAL;
  const int fd = io.open(io.context, path, false);
  if (fd < 0)
    return fd;
  int code = io.info(io.context, fd, info);
  const int closed = io.close(io.context, fd);
  if (code >= 0 && closed < 0)
    code = closed;
  return code;
}
bool identity(const Info &a, const Info &b) {
  return a.device == b.device && a.inode == b.inode && a.regular == b.regular &&
         a.directory == b.directory;
}
bool unchanged(const Info &a, const Info &b) {
  return identity(a, b) && a.size == b.size && a.modified == b.modified &&
         a.modified_ns == b.modified_ns && a.changed == b.changed &&
         a.changed_ns == b.changed_ns;
}
bool Clipboard::copy(const char *source, const Io *provided) {
  Info value{};
  error = inspect(source, value, provided);
  if (error < 0 || !value.regular || !value.inode ||
      std::strlen(leaf(source)) > 255) {
    if (error >= 0)
      error = -ENOTSUP;
    return false;
  }
  std::snprintf(path, sizeof(path), "%s", source);
  info = value;
  ready = true;
  error = 0;
  return true;
}
void Transfer::close_handles() {
  if (io) {
    if (input >= 0)
      io->close(io->context, input);
    if (output >= 0)
      io->close(io->context, output);
  }
  input = output = -1;
}
bool Transfer::fail(const char *where, int code) {
  close_handles();
  state = State::failed;
  error = code < 0 ? -code : code ? code : EIO;
  std::snprintf(stage, sizeof(stage), "%s", where);
  return false;
}
bool Transfer::begin(const Clipboard &clipboard, const char *folder,
                     const Io *provided) {
  if (busy())
    return false;
  close_handles();
  io = provided ? provided : &defaults;
  state = State::idle;
  copied = verified = total = 0;
  error = 0;
  destination[0] = stage[0] = 0;
  destination_created = false;
  if (!clipboard.ready || !folder || !*folder ||
      std::strlen(folder) >= sizeof(directory) || !usable(*io))
    return fail("Clipboard", EINVAL);
  std::snprintf(source, sizeof(source), "%s", clipboard.path);
  std::snprintf(directory, sizeof(directory), "%s", folder);
  int code = inspect(directory, parent, io);
  if (code < 0 || !parent.directory || !parent.inode)
    return fail("Destination folder", code < 0 ? code : ENOTSUP);
  input = io->open(io->context, source, false);
  if (input < 0)
    return fail("Source open", input);
  code = io->info(io->context, input, initial);
  if (code < 0 || !initial.regular || !initial.inode)
    return fail("Regular files only", code < 0 ? code : ENOTSUP);
  if (!unchanged(initial, clipboard.info))
    return fail("Source changed", ChangedPath);
  if (initial.size > MaximumBytes)
    return fail("8 GB copy limit", EFBIG);
  total = initial.size;
  for (int number = 0; number < 100; ++number) {
    if (!destination_name(destination, sizeof(destination), directory,
                          leaf(source), number))
      return fail("Destination path", ENAMETOOLONG);
    output = io->open(io->context, destination, true);
    if (output >= 0 || output != -EEXIST)
      break;
  }
  if (output < 0)
    return fail("Destination create", output);
  destination_created = true;
  code = io->info(io->context, output, created);
  Info actual_parent{};
  if (code < 0 || !created.regular || !created.inode)
    return fail("Destination identity", code < 0 ? code : ENOTSUP);
  code = inspect(directory, actual_parent, io);
  if (code < 0 || !identity(parent, actual_parent))
    return fail("Folder changed", code < 0 ? code : ChangedPath);
  state = State::copying;
  return true;
}
void Transfer::cancel() {
  if (!busy())
    return;
  close_handles();
  state = State::cancelled;
  std::snprintf(stage, sizeof(stage), "Cancelled");
}
void Transfer::step() {
  if (!busy())
    return;
  if (state == State::copying) {
    const size_t count =
        total - copied < Chunk ? size_t(total - copied) : Chunk;
    int code = read_exact(*io, input, bytes, count);
    if (code < 0) {
      fail("Source read", code);
      return;
    }
    size_t sent = 0;
    while (sent < count) {
      const auto wrote =
          io->write(io->context, output, bytes + sent, count - sent);
      if (wrote < 0 || !wrote || uint64_t(wrote) > count - sent) {
        fail("Destination write", wrote < 0 ? int(wrote) : EIO);
        return;
      }
      sent += size_t(wrote);
    }
    copied += count;
    if (copied != total)
      return;
    unsigned char extra = 0;
    const auto beyond = io->read(io->context, input, &extra, 1);
    if (beyond != 0) {
      fail("Source size changed", beyond < 0 ? int(beyond) : ChangedPath);
      return;
    }
    code = io->sync(io->context, output);
    if (code < 0) {
      fail("Flush copy", code);
      return;
    }
    code = io->rewind(io->context, input);
    if (code >= 0)
      code = io->rewind(io->context, output);
    if (code < 0) {
      fail("Verification seek", code);
      return;
    }
    state = State::verifying;
    return;
  }
  const size_t count =
      total - verified < Chunk ? size_t(total - verified) : Chunk;
  int code = read_exact(*io, input, bytes, count);
  if (code >= 0)
    code = read_exact(*io, output, compare, count);
  if (code < 0 || std::memcmp(bytes, compare, count)) {
    fail("Verify contents", code < 0 ? code : EIO);
    return;
  }
  verified += count;
  if (verified != total)
    return;
  unsigned char extra = 0;
  const auto source_extra = io->read(io->context, input, &extra, 1);
  const auto destination_extra = io->read(io->context, output, &extra, 1);
  if (source_extra != 0 || destination_extra != 0) {
    fail("Verify exact size", source_extra < 0 ? int(source_extra)
                             : destination_extra < 0 ? int(destination_extra)
                                                     : ChangedPath);
    return;
  }
  Info source_now{}, source_named{}, output_now{}, output_named{}, parent_now{};
  code = io->info(io->context, input, source_now);
  if (code >= 0)
    code = inspect(source, source_named, io);
  if (code < 0 || !unchanged(initial, source_now) ||
      !unchanged(initial, source_named)) {
    fail("Source changed", code < 0 ? code : ChangedPath);
    return;
  }
  code = io->info(io->context, output, output_now);
  if (code >= 0)
    code = inspect(destination, output_named, io);
  if (code >= 0)
    code = inspect(directory, parent_now, io);
  if (code < 0 || !identity(created, output_now) ||
      !identity(created, output_named) || output_now.size != total ||
      output_named.size != total || !identity(parent, parent_now)) {
    fail("Destination changed", code < 0 ? code : ChangedPath);
    return;
  }
  code = io->permissions(io->context, output, initial.mode);
  if (code >= 0)
    code = io->sync(io->context, output);
  if (code < 0) {
    fail("Copy permissions/flush", code);
    return;
  }
  const int a = input, b = output;
  input = output = -1;
  code = io->close(io->context, a);
  const int closed = io->close(io->context, b);
  if (code < 0 || closed < 0) {
    fail("Copy close", code < 0 ? code : closed);
    return;
  }
  state = State::complete;
  std::snprintf(stage, sizeof(stage), "Copy complete");
}
} // namespace ww::fileops
