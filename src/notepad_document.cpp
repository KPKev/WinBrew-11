// SPDX-License-Identifier: GPL-3.0-or-later
#include "notepad_document.hpp"
#include "privilege.hpp"
#include "explorer_directory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
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
int64_t sceKernelWrite(int, const void *, size_t);
int sceKernelFchmod(int, unsigned);
int sceKernelFsync(int);
int sceKernelClose(int);
int sceKernelRename(const char *, const char *);
int sceKernelUnlink(const char *);
}
#endif
namespace ww {
namespace {
int normalized(int value) {
#ifdef __PROSPERO__
  return value < 0 ? -directory::native_error(value) : value;
#else
  return value < 0 ? -directory::error_or_io(errno) : value;
#endif
}
int create_file(void *, const char *path, unsigned mode) {
#ifdef __PROSPERO__
  return normalized(ww::privilege::run(
      [&] { return sceKernelOpen(path, O_WRONLY | O_CREAT | O_EXCL, int(mode)); }));
#elif defined(_WIN32)
  return normalized(_open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                          _S_IREAD | _S_IWRITE));
#else
  return normalized(ww::privilege::run([&] { return ::open(path, O_WRONLY | O_CREAT | O_EXCL, mode); }));
#endif
}
int64_t write_file(void *, int fd, const char *data, size_t size) {
#ifdef __PROSPERO__
  const auto result = sceKernelWrite(fd, data, size);
  return result < 0 ? normalized(int(result)) : result;
#elif defined(_WIN32)
  const auto result = _write(fd, data, unsigned(size));
  return result < 0 ? normalized(result) : result;
#else
  const auto result = ::write(fd, data, size);
  return result < 0 ? normalized(int(result)) : result;
#endif
}
int set_permissions(void *, int fd, unsigned mode) {
#ifdef __PROSPERO__
  return normalized(ww::privilege::run([&] { return sceKernelFchmod(fd, mode); }));
#elif defined(_WIN32)
  (void)fd;
  (void)mode;
  return 0; // The replacement preserves a regular writable Windows file.
#else
  return normalized(ww::privilege::run([&] { return ::fchmod(fd, mode); }));
#endif
}
int sync_file(void *, int fd) {
#ifdef __PROSPERO__
  return normalized(sceKernelFsync(fd));
#elif defined(_WIN32)
  return normalized(_commit(fd));
#else
  return normalized(::fsync(fd));
#endif
}
int close_file(void *, int fd) {
#ifdef __PROSPERO__
  return normalized(sceKernelClose(fd));
#elif defined(_WIN32)
  return normalized(_close(fd));
#else
  return normalized(::close(fd));
#endif
}
int replace_file(void *, const char *from, const char *to) {
#ifdef __PROSPERO__
  return normalized(ww::privilege::run([&] { return sceKernelRename(from, to); }));
#elif defined(_WIN32)
  return MoveFileExA(from, to,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
             ? 0
             : -EIO;
#else
  return normalized(ww::privilege::run([&] { return std::rename(from, to); }));
#endif
}
int remove_file(void *, const char *path) {
#ifdef __PROSPERO__
  return normalized(ww::privilege::run([&] { return sceKernelUnlink(path); }));
#else
  return normalized(ww::privilege::run([&] { return std::remove(path); }));
#endif
}
const DocumentIo real_io{nullptr,   create_file, write_file,   set_permissions,
                         sync_file, close_file,  replace_file, remove_file};
// A single UI thread owns the editor. Large reads use BSS instead of the PS5
// startup stack; committing to the document happens only after validation.
unsigned char loaded[TextDocument::Capacity + 4];
uint32_t save_serial = 0;
const unsigned char bom_bytes[] = {0xef, 0xbb, 0xbf};
bool matches(const char *path, const char *body, size_t length, bool bom,
             int &error, unsigned *permissions = nullptr,
             bool *readonly = nullptr) {
  FILE *file = nullptr;
  ww::privilege::run([&] { file = std::fopen(path, "rb"); return file ? 0 : -1; });
  if (!file) {
    error = directory::error_or_io(errno);
    return false;
  }
#ifdef _WIN32
  struct _stat info{};
  if (_fstat(_fileno(file), &info) == 0) {
    if (permissions)
      *permissions = unsigned(info.st_mode & 0777);
    if (readonly)
      *readonly = (info.st_mode & _S_IWRITE) == 0;
  }
#else
  struct stat info{};
  if (fstat(fileno(file), &info) == 0) {
    if (permissions)
      *permissions = unsigned(info.st_mode & 0777);
    if (readonly)
      *readonly = (info.st_mode & 0222) == 0;
  }
#endif
  bool equal = true;
  if (bom) {
    unsigned char prefix[3]{};
    equal = std::fread(prefix, 1, 3, file) == 3 &&
            !std::memcmp(prefix, bom_bytes, 3);
  }
  char chunk[2048];
  size_t offset = 0;
  while (equal && offset < length) {
    const size_t wanted =
        length - offset < sizeof(chunk) ? length - offset : sizeof(chunk);
    const size_t got = std::fread(chunk, 1, wanted, file);
    equal = got == wanted && !std::memcmp(chunk, body + offset, wanted);
    offset += got;
  }
  if (equal)
    equal = std::fgetc(file) == EOF;
  const bool failed = std::ferror(file) != 0;
  const int read_error = errno;
  const bool closed = std::fclose(file) == 0;
  error = failed || !closed ? directory::error_or_io(read_error) : 0;
  return equal && !failed && closed;
}
size_t newline_size(const TextDocument &d, size_t at) {
  return at < d.length && d.text[at] == '\r' && at + 1 < d.length &&
                 d.text[at + 1] == '\n'
             ? 2
             : 1;
}
size_t line_start(const TextDocument &d, size_t at) {
  while (at > 0 && d.text[at - 1] != '\r' && d.text[at - 1] != '\n')
    --at;
  return at;
}
size_t line_end(const TextDocument &d, size_t at) {
  while (at < d.length && d.text[at] != '\r' && d.text[at] != '\n')
    ++at;
  return at;
}
} // namespace
const DocumentIo &document_io() { return real_io; }
const char *document_result_message(DocumentResult result) {
  switch (result) {
  case DocumentResult::ok:
    return "Saved";
  case DocumentResult::unsaved:
    return "Save your changes before closing this file.";
  case DocumentResult::invalid_path:
    return "This file path cannot be opened.";
  case DocumentResult::open:
    return "This file could not be opened.";
  case DocumentResult::read:
    return "The file could not be read completely.";
  case DocumentResult::too_large:
    return "This file exceeds Notepad's 64 KB editing limit. It was not "
           "changed.";
  case DocumentResult::binary:
    return "This is not a supported text file. It was not changed.";
  case DocumentResult::encoding:
    return "This file uses unsupported characters. ASCII text and UTF-8 BOM "
           "are supported.";
  case DocumentResult::read_only:
    return "This file is read-only. Your changes have not been saved.";
  case DocumentResult::conflict:
    return "The file changed outside Notepad. Close and reopen it before "
           "editing.";
  case DocumentResult::create:
    return "A temporary file could not be created. The original is unchanged.";
  case DocumentResult::write:
    return "The edited text could not be written. The original is unchanged.";
  case DocumentResult::permissions:
    return "File permissions could not be preserved. The original is "
           "unchanged.";
  case DocumentResult::sync:
    return "The edited file could not be flushed to storage. The original is "
           "unchanged.";
  case DocumentResult::close:
    return "The temporary file could not be closed safely. The original is "
           "unchanged.";
  case DocumentResult::verify:
    return "The edited file failed verification. The original is unchanged.";
  case DocumentResult::replace:
    return "The original file could not be replaced. Your edits remain open.";
  case DocumentResult::capacity:
    return "The 64 KB editing limit has been reached.";
  }
  return "This action could not be completed.";
}
bool TextDocument::fail(DocumentResult state, int code) {
  result = state;
  error = code;
  return false;
}
bool TextDocument::dirty() const {
  return active && (length != original_length ||
                    std::memcmp(text, original, length) != 0);
}
bool TextDocument::load(const char *name, bool discard) {
  if (dirty() && !discard)
    return fail(DocumentResult::unsaved);
  if (!name || !*name || std::strlen(name) >= sizeof(path))
    return fail(DocumentResult::invalid_path);
  char name_copy[1024];
  std::snprintf(name_copy, sizeof(name_copy), "%s", name);
  FILE *file = nullptr;
  ww::privilege::run([&] { file = std::fopen(name_copy, "rb"); return file ? 0 : -1; });
  if (!file)
    return fail(DocumentResult::open, directory::error_or_io(errno));
  const size_t count = std::fread(loaded, 1, sizeof(loaded), file);
  const bool readable = std::ferror(file) == 0;
  int saved_error = errno;
  unsigned permissions = 0600;
  bool readonly = false;
#ifdef _WIN32
  struct _stat info{};
  if (_fstat(_fileno(file), &info) == 0) {
    readonly = (info.st_mode & _S_IWRITE) == 0;
    permissions = unsigned(info.st_mode & 0777);
  }
#else
  struct stat info{};
  if (fstat(fileno(file), &info) == 0) {
    readonly = (info.st_mode & 0222) == 0;
    permissions = unsigned(info.st_mode & 0777);
  }
#endif
  const bool closed = std::fclose(file) == 0;
  if (!readable || !closed)
    return fail(DocumentResult::read, directory::error_or_io(saved_error));
  const bool has_bom = count >= 3 && !std::memcmp(loaded, bom_bytes, 3);
  const size_t start = has_bom ? 3 : 0, size = count - start;
  if (size > Capacity)
    return fail(DocumentResult::too_large);
  auto ending = Newline::lf;
  bool first_ending = true;
  for (size_t i = start; i < count; ++i) {
    const auto value = loaded[i];
    if (value >= 128)
      return fail(DocumentResult::encoding);
    if ((value < 32 && value != '\t' && value != '\r' && value != '\n') ||
        value == 127)
      return fail(DocumentResult::binary);
    if (first_ending && (value == '\r' || value == '\n')) {
      ending = value == '\n'                            ? Newline::lf
               : i + 1 < count && loaded[i + 1] == '\n' ? Newline::crlf
                                                        : Newline::cr;
      first_ending = false;
    }
  }
  std::memcpy(text, loaded + start, size);
  text[size] = 0;
  std::memcpy(original, text, size + 1);
  std::snprintf(path, sizeof(path), "%s", name_copy);
  length = original_length = size;
  caret = 0;
  mode = permissions;
  read_only = readonly;
  bom = has_bom;
  newline = ending;
  active = true;
  result = DocumentResult::ok;
  error = 0;
  return true;
}
bool TextDocument::save(const DocumentIo *provided) {
  if (!active || !path[0])
    return fail(DocumentResult::invalid_path);
  if (!dirty()) {
    result = DocumentResult::ok;
    error = 0;
    return true;
  }
  if (read_only)
    return fail(DocumentResult::read_only, EACCES);
  const auto &io = provided ? *provided : real_io;
  if (!io.create || !io.write || !io.permissions || !io.sync || !io.close ||
      !io.replace || !io.remove)
    return fail(DocumentResult::write, EINVAL);
  int checked_error = 0;
  unsigned current_mode = mode;
  bool current_readonly = false;
  if (!matches(path, original, original_length, bom, checked_error,
               &current_mode, &current_readonly))
    return fail(checked_error ? DocumentResult::read : DocumentResult::conflict,
                checked_error);
  if (current_readonly)
    return fail(DocumentResult::read_only, EACCES);
  char temporary[1120];
  const char *slash = std::strrchr(path, '/');
#ifdef _WIN32
  const char *backslash = std::strrchr(path, '\\');
  if (backslash && (!slash || backslash > slash))
    slash = backslash;
#endif
  const int parent_size = slash ? int(slash - path + 1) : 0;
  int fd = -1;
  for (int attempt = 0; attempt < 32; ++attempt) {
    std::snprintf(temporary, sizeof(temporary), "%.*s.wine11-note-%llx-%x.tmp",
                  parent_size, path,
                  static_cast<unsigned long long>(std::time(nullptr)),
                  ++save_serial);
    fd = io.create(io.context, temporary, current_mode);
    if (fd >= 0 || fd != -EEXIST)
      break;
  }
  if (fd < 0)
    return fail(DocumentResult::create, -fd);
  auto abandon = [&](DocumentResult state, int code) {
    if (fd >= 0)
      io.close(io.context, fd);
    io.remove(io.context, temporary);
    return fail(state, code);
  };
  auto write_all = [&](const char *bytes, size_t size) {
    size_t sent = 0;
    while (sent < size) {
      const auto count = io.write(io.context, fd, bytes + sent, size - sent);
      if (count < 0)
        return int(-count);
      if (!count || uint64_t(count) > size - sent)
        return EIO;
      sent += size_t(count);
    }
    return 0;
  };
  int code = bom ? write_all(reinterpret_cast<const char *>(bom_bytes), 3) : 0;
  if (!code)
    code = write_all(text, length);
  if (code)
    return abandon(DocumentResult::write, code);
  code = io.permissions(io.context, fd, current_mode);
  if (code < 0)
    return abandon(DocumentResult::permissions, -code);
  code = io.sync(io.context, fd);
  if (code < 0)
    return abandon(DocumentResult::sync, -code);
  const int finished = fd;
  fd = -1;
  code = io.close(io.context, finished);
  if (code < 0)
    return abandon(DocumentResult::close, -code);
  if (!matches(temporary, text, length, bom, checked_error))
    return abandon(DocumentResult::verify, checked_error ? checked_error : EIO);
  unsigned final_mode = current_mode;
  current_readonly = false;
  if (!matches(path, original, original_length, bom, checked_error, &final_mode,
               &current_readonly))
    return abandon(checked_error ? DocumentResult::read
                                 : DocumentResult::conflict,
                   checked_error);
  if (current_readonly)
    return abandon(DocumentResult::read_only, EACCES);
  if (final_mode != current_mode)
    return abandon(DocumentResult::conflict, EAGAIN);
  code = io.replace(io.context, temporary, path);
  if (code < 0)
    return abandon(DocumentResult::replace, -code);
  // The atomic replacement is the commit point. All fallible verification and
  // flushing happened before it; no reported save failure follows this point.
  std::memcpy(original, text, length + 1);
  original_length = length;
  mode = current_mode;
  result = DocumentResult::ok;
  error = 0;
  return true;
}
bool TextDocument::close(bool discard) {
  if (dirty() && !discard)
    return fail(DocumentResult::unsaved);
  text[0] = original[0] = path[0] = 0;
  length = original_length = caret = 0;
  active = false;
  result = DocumentResult::ok;
  error = 0;
  return true;
}
bool TextDocument::insert(char value) {
  if (!active ||
      (value != '\n' && value != '\t' && (value < 32 || value > 126)))
    return false;
  const char bytes[] = {value == '\n' && newline != Newline::lf ? '\r' : value,
                        '\n'};
  const size_t size = value == '\n' && newline == Newline::crlf ? 2 : 1;
  if (length + size > Capacity)
    return fail(DocumentResult::capacity);
  if (caret > length)
    caret = length;
  std::memmove(text + caret + size, text + caret, length - caret + 1);
  std::memcpy(text + caret, bytes, size);
  caret += size;
  length += size;
  result = DocumentResult::ok;
  error = 0;
  return true;
}
void TextDocument::erase(bool forward) {
  if (!active || !length)
    return;
  if (caret > length)
    caret = length;
  size_t at = caret;
  if (!forward) {
    if (!at)
      return;
    --at;
    if (at && text[at] == '\n' && text[at - 1] == '\r')
      --at;
  }
  if (at >= length)
    return;
  const size_t count = newline_size(*this, at);
  std::memmove(text + at, text + at + count, length - at - count + 1);
  length -= count;
  caret = at;
}
void TextDocument::clear() {
  if (!active)
    return;
  text[0] = 0;
  length = caret = 0;
}
void TextDocument::move(int delta) {
  if (caret > length)
    caret = length;
  if (delta < 0 && caret) {
    --caret;
    if (caret && text[caret] == '\n' && text[caret - 1] == '\r')
      --caret;
  } else if (delta > 0 && caret < length)
    caret += newline_size(*this, caret);
}
void TextDocument::home(bool whole) {
  caret = whole ? 0 : line_start(*this, caret);
}
void TextDocument::end(bool whole) {
  caret = whole ? length : line_end(*this, caret);
}
void TextDocument::vertical(int lines) {
  if (caret > length)
    caret = length;
  size_t start = line_start(*this, caret), column = caret - start;
  while (lines < 0 && start) {
    size_t previous = start - 1;
    if (previous && text[previous] == '\n' && text[previous - 1] == '\r')
      --previous;
    start = line_start(*this, previous);
    ++lines;
  }
  while (lines > 0) {
    const size_t finish = line_end(*this, start);
    if (finish == length)
      break;
    start = finish + newline_size(*this, finish);
    --lines;
  }
  const size_t finish = line_end(*this, start);
  caret = column > finish - start ? finish : start + column;
}
} // namespace ww
