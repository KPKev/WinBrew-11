// SPDX-License-Identifier: GPL-3.0-or-later
#include "notepad_document.hpp"
#include "privilege.hpp"
#include "explorer_directory.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
  return active && (bom != original_bom || length != original_length ||
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
  bom = original_bom = has_bom;
  newline = ending;
  active = true;
  undo_history.clear(); redo_history.clear();
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
  if (!matches(path, original, original_length, original_bom, checked_error,
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
  if (!matches(path, original, original_length, original_bom, checked_error, &final_mode,
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
  original_bom = bom;
  mode = current_mode;
  result = DocumentResult::ok;
  error = 0;
  return true;
}
bool TextDocument::save_as(const char *name, const TextDocument *confirmed,
                           const DocumentIo *provided) {
  if(!active || !name || !*name || std::strlen(name)>=sizeof(path))return fail(DocumentResult::invalid_path);
  if(!std::strcmp(name,path) && !confirmed)return save(provided);
  const auto &io=provided?*provided:real_io;
  if(!io.create || !io.sync || !io.close)return fail(DocumentResult::create,EINVAL);
  auto target=std::make_unique<TextDocument>();
  if(confirmed){
    if(!confirmed->active || std::strcmp(confirmed->path,name))return fail(DocumentResult::conflict);
    *target=*confirmed;
  }else{
    // Reserve the final name exclusively. Saving then uses the same verified,
    // atomic replacement and conflict check as an ordinary document save.
    const unsigned new_mode=mode|0600;
    const int fd=io.create(io.context,name,new_mode);
    if(fd<0)return fail(fd==-EEXIST?DocumentResult::conflict:DocumentResult::create,-fd);
    const int synced=io.sync(io.context,fd),closed=io.close(io.context,fd);
    if(synced<0 || closed<0)return fail(DocumentResult::sync,synced<0?-synced:-closed);
    target->active=true;target->mode=new_mode;
    std::snprintf(target->path,sizeof(target->path),"%s",name);
  }
  std::memcpy(target->text,text,length+1);target->length=length;target->caret=caret;
  target->bom=bom;target->newline=newline;
  int checked=0;
  if(!target->dirty() && !matches(target->path,target->original,target->original_length,target->original_bom,checked))return fail(DocumentResult::conflict,checked);
  if(!target->save(provided))return fail(target->result,target->error);
  auto undo_saved=std::move(undo_history), redo_saved=std::move(redo_history);
  *this=*target;undo_history=std::move(undo_saved);redo_history=std::move(redo_saved);return true;
}
bool TextDocument::close(bool discard) {
  if (dirty() && !discard)
    return fail(DocumentResult::unsaved);
  text[0] = original[0] = path[0] = 0;
  length = original_length = caret = 0;
  active = false;
  undo_history.clear(); redo_history.clear();
  result = DocumentResult::ok;
  error = 0;
  return true;
}
void TextDocument::checkpoint() {
  if (!active) return;
  if (undo_history.size() == 32) undo_history.erase(undo_history.begin());
  undo_history.push_back({std::string(text,length),caret});
  redo_history.clear();
}
bool TextDocument::undo() {
  if (!active || undo_history.empty()) return false;
  if (redo_history.size()==32) redo_history.erase(redo_history.begin());
  redo_history.push_back({std::string(text,length),caret});
  EditState state=std::move(undo_history.back());undo_history.pop_back();
  length=state.text.size();std::memcpy(text,state.text.data(),length);text[length]=0;caret=std::min(state.caret,length);return true;
}
bool TextDocument::redo() {
  if (!active || redo_history.empty()) return false;
  if (undo_history.size()==32) undo_history.erase(undo_history.begin());
  undo_history.push_back({std::string(text,length),caret});
  EditState state=std::move(redo_history.back());redo_history.pop_back();
  length=state.text.size();std::memcpy(text,state.text.data(),length);text[length]=0;caret=std::min(state.caret,length);return true;
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
  checkpoint();
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
  checkpoint();
  std::memmove(text + at, text + at + count, length - at - count + 1);
  length -= count;
  caret = at;
}
void TextDocument::clear() {
  if (!active)
    return;
  if (length) checkpoint();
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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
namespace ww {
namespace {
std::string sheet_csv(const std::array<std::string, 2600> &cells) {
  int last_row = 0, last_col = 0;
  for (int r = 0; r < 100; ++r)
    for (int c = 0; c < 26; ++c)
      if (!cells[r * 26 + c].empty()) {
        last_row = std::max(r, last_row);
        last_col = std::max(c, last_col);
      }
  std::string out;
  for (int r = 0; r <= last_row; ++r) {
    for (int c = 0; c <= last_col; ++c) {
      if (c)
        out += ',';
      const auto &v = cells[r * 26 + c];
      const bool quote = v.find_first_of(",\"\r\n") != std::string::npos;
      if (quote)
        out += '"';
      for (char ch : v) {
        out += ch;
        if (quote && ch == '"')
          out += '"';
      }
      if (quote)
        out += '"';
    }
    out += "\r\n";
  }
  return out;
}
struct Formula {
  const SheetDocument &sheet;
  const char *p;
  int depth, &budget;
  std::string &error;
  void space() {
    while (*p == ' ')
      ++p;
  }
  bool take(char c) {
    space();
    if (*p != c)
      return false;
    ++p;
    return true;
  }
  bool reference(int &r, int &c) {
    space();
    const char *start = p;
    if (*p == '$')
      ++p;
    if (!std::isalpha((unsigned char)*p)) {
      p = start;
      return false;
    }
    c = std::toupper((unsigned char)*p++) - 'A';
    if (*p == '$')
      ++p;
    if (!std::isdigit((unsigned char)*p)) {
      p = start;
      return false;
    }
    r = 0;
    while (std::isdigit((unsigned char)*p)) {
      r = r * 10 + *p++ - '0';
      if (r > 1000)
        break;
    }
    --r;
    if (r < 0 || r >= 100 || c < 0 || c >= 26)
      error = "#REF!";
    return true;
  }
  double atom() {
    if (--budget < 0 || depth > 32) {
      error = "#CYCLE!";
      return 0;
    }
    space();
    if (take('+')) {
      Formula child{sheet, p, depth + 1, budget, error};
      double v = child.atom();
      p = child.p;
      return v;
    }
    if (take('-')) {
      Formula child{sheet, p, depth + 1, budget, error};
      double v = child.atom();
      p = child.p;
      return -v;
    }
    if (take('(')) {
      Formula child{sheet, p, depth + 1, budget, error};
      double v = child.expression();
      p = child.p;
      if (!take(')'))
        error = "#VALUE!";
      return v;
    }
    const char *start = p;
    std::string name;
    while (std::isalpha((unsigned char)*p))
      name += char(std::toupper((unsigned char)*p++));
    if (!name.empty() && take('(')) {
      if (name != "SUM" && name != "AVERAGE" && name != "MIN" &&
          name != "MAX" && name != "COUNT") {
        error = "#NAME?";
        return 0;
      }
      double total = 0, low = 0, high = 0;
      int count = 0;
      auto add = [&](double v) {
        if (!count)
          low = high = v;
        low = std::min(low, v);
        high = std::max(high, v);
        total += v;
        ++count;
      };
      do {
        const char *before = p;
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
        if (reference(r1, c1) && take(':')) {
          if (!reference(r2, c2) || !error.empty()) {
            error = "#REF!";
            return 0;
          }
          for (int r = std::min(r1, r2); r <= std::max(r1, r2); ++r)
            for (int c = std::min(c1, c2); c <= std::max(c1, c2); ++c) {
              const auto &raw = sheet.cells[r * 26 + c];
              if (raw.empty())
                continue;
              char *end = nullptr;
              std::strtod(raw.c_str(), &end);
              if (raw[0] != '=' && (end == raw.c_str() || *end))
                continue;
              add(sheet.number(r, c, depth + 1, budget, error));
            }
        } else {
          p = before;
          Formula child{sheet, p, depth + 1, budget, error};
          add(child.expression());
          p = child.p;
        }
        if (!error.empty())
          return 0;
      } while (take(','));
      if (!take(')'))
        error = "#VALUE!";
      if (name == "COUNT")
        return count;
      if (name == "AVERAGE") {
        if (!count) {
          error = "#DIV/0!";
          return 0;
        }
        return total / count;
      }
      return name == "MIN" ? low : name == "MAX" ? high : total;
    }
    p = start;
    int r = 0, c = 0;
    if (reference(r, c))
      return error.empty() ? sheet.number(r, c, depth + 1, budget, error) : 0;
    char *end = nullptr;
    double v = std::strtod(p, &end);
    if (end == p) {
      error = "#VALUE!";
      if (*p)
        ++p;
      return 0;
    }
    p = end;
    return v;
  }
  double product() {
    double v = atom();
    for (;;) {
      space();
      char op = *p;
      if (op != '*' && op != '/')
        break;
      ++p;
      double b = atom();
      if (op == '/' && b == 0) {
        error = "#DIV/0!";
        return 0;
      }
      v = op == '*' ? v * b : v / b;
      if (!error.empty())
        break;
    }
    return v;
  }
  double expression() {
    double v = product();
    for (;;) {
      space();
      char op = *p;
      if (op != '+' && op != '-')
        break;
      ++p;
      double b = product();
      v = op == '+' ? v + b : v - b;
      if (!error.empty())
        break;
    }
    return v;
  }
};
} // namespace
bool SheetDocument::load(const char *path) {
  const char *extension = path ? std::strrchr(path, '.') : nullptr;
  if (extension) {
    std::string ext = extension;
    for (char &c : ext)
      c = char(std::tolower((unsigned char)c));
    if (ext == ".xlsx")
      return load_xlsx(path);
  }
  if (dirty()) {
    std::snprintf(status, sizeof(status),
                  "Save the current worksheet before opening another.");
    return false;
  }
  // Load into a separate heap document: malformed CSV must not replace the open
  // sheet.
  auto candidate = new TextDocument;
  if (!candidate->load(path)) {
    std::snprintf(status, sizeof(status), "%s",
                  document_result_message(candidate->result));
    delete candidate;
    return false;
  }
  std::array<std::string, 2600> parsed{};
  int r = 0, c = 0;
  size_t at = 0;
  bool valid = true;
  while (at < candidate->length && valid) {
    if (r >= Rows || c >= Columns) {
      valid = false;
      break;
    }
    std::string value;
    bool quoted = candidate->text[at] == '"';
    if (quoted) {
      ++at;
      bool closed = false;
      while (at < candidate->length) {
        char ch = candidate->text[at++];
        if (ch == '"') {
          if (at < candidate->length && candidate->text[at] == '"')
            ++at;
          else {
            closed = true;
            break;
          }
        }
        value += ch;
      }
      if (!closed)
        valid = false;
    } else
      while (at < candidate->length && candidate->text[at] != ',' &&
             candidate->text[at] != '\r' && candidate->text[at] != '\n') {
        char ch = candidate->text[at++];
        if (ch == '"')
          valid = false;
        value += ch;
      }
    if (value.size() > 255)
      valid = false;
    parsed[r * Columns + c] = std::move(value);
    if (at == candidate->length)
      break;
    char delimiter = candidate->text[at++];
    if (delimiter == ',') {
      ++c;
      if (at == candidate->length && c >= Columns)
        valid = false;
    } else if (delimiter == '\r' || delimiter == '\n') {
      if (delimiter == '\r' && at < candidate->length &&
          candidate->text[at] == '\n')
        ++at;
      ++r;
      c = 0;
    } else
      valid = false;
  }
  if (!valid) {
    std::snprintf(status, sizeof(status),
                  "CSV unsupported: use up to 100 rows, 26 columns and 255 "
                  "characters per cell.");
    delete candidate;
    return false;
  }
  xlsx_format = false;
  selected_sheet = 0;
  workbook = xlsx::Workbook{};
  storage = *candidate;
  delete candidate;
  cells = std::move(parsed);
  cached.fill(false);
  row = column = first_row = first_column = 0;
  editing = false;
  std::snprintf(status, sizeof(status), "Ready");
  return true;
}
bool SheetDocument::set(int r, int c, const char *value) {
  if (r < 0 || r >= Rows || c < 0 || c >= Columns || !value ||
      std::strlen(value) > 255)
    return false;
  const auto old = cells[r * Columns + c];
  if (old == value)
    return true;
  if (xlsx_format) {
    int old_row = row, old_column = column;
    row = r;
    column = c;
    bool allowed = editable();
    row = old_row;
    column = old_column;
    if (!allowed) {
      std::snprintf(status, sizeof(status),
                    "This XLSX cell is preserved but cannot be edited here.");
      return false;
    }
  }
  cells[r * Columns + c] = value;
  if (xlsx_format) {
    workbook.pages[selected_sheet].cells[r * Columns + c] = value;
    workbook.pages[selected_sheet].formulas[r * Columns + c] = value[0] == '=';
    workbook.pages[selected_sheet].cached_values.erase(xlsx::address(r, c));
    workbook.changed = true;
    cached.fill(false);
    return true;
  }
  const auto csv = sheet_csv(cells);
  if (csv.size() > TextDocument::Capacity) {
    cells[r * Columns + c] = old;
    std::snprintf(
        status, sizeof(status),
        "Worksheet exceeds the 64 KB CSV limit. Edit retained in formula bar.");
    return false;
  }
  if (old == value)
    return true;
  cached.fill(false);
  std::memcpy(storage.text, csv.c_str(), csv.size() + 1);
  storage.length = csv.size();
  storage.active = true;
  return true;
}
bool SheetDocument::save(const DocumentIo *provided) {
  if (xlsx_format)
    return save_xlsx(provided);
  bool ok = storage.save(provided);
  std::snprintf(status, sizeof(status), "%s",
                document_result_message(storage.result));
  return ok;
}
double SheetDocument::number(int r, int c, int depth, int &budget,
                             std::string &error) const {
  if (r < 0 || r >= Rows || c < 0 || c >= Columns) {
    error = "#REF!";
    return 0;
  }
  if (depth > 32 || --budget < 0) {
    error = "#CYCLE!";
    return 0;
  }
  const auto &v = cells[r * Columns + c];
  if (v.empty())
    return 0;
  if (v[0] == '=') {
    Formula formula{*this, v.c_str() + 1, depth, budget, error};
    double n = formula.expression();
    formula.space();
    if (*formula.p && error.empty())
      error = "#VALUE!";
    if (!std::isfinite(n))
      error = "#NUM!";
    return n;
  }
  char *end = nullptr;
  double n = std::strtod(v.c_str(), &end);
  while (end && *end == ' ')
    ++end;
  if (end == v.c_str() || (end && *end))
    error = "#VALUE!";
  if (!std::isfinite(n))
    error = "#NUM!";
  return n;
}
std::string SheetDocument::display(int r, int c) const {
  const auto &raw = cells[r * Columns + c];
  if (raw.empty() || raw[0] != '=' ||
      (xlsx_format &&
       !workbook.pages[selected_sheet].formulas[r * Columns + c]))
    return raw;
  int index = r * Columns + c;
  if (cached[index])
    return calculated[index];
  int budget = 10000;
  std::string error;
  double n = number(r, c, 0, budget, error);
  char out[80];
  std::snprintf(out, sizeof(out), "%.12g", n);
  calculated[index] = error.empty() ? out : error;
  if (xlsx_format &&
      (error == "#NAME?" || error == "#VALUE!" || error == "#REF!") &&
      raw == workbook.pages[selected_sheet].original[index]) {
    auto found =
        workbook.pages[selected_sheet].cached_values.find(xlsx::address(r, c));
    if (found != workbook.pages[selected_sheet].cached_values.end() &&
        !found->second.empty())
      calculated[index] = found->second + " (cached)";
  }
  cached[index] = true;
  return calculated[index];
}
} // namespace ww
namespace ww {
namespace {
bool workbook_read(const char *path, std::string &bytes, TextDocument &state) {
  FILE *f = nullptr;
  ww::privilege::run([&] {
    f = std::fopen(path, "rb");
    return f ? 0 : -1;
  });
  if (!f)
    return state.fail(DocumentResult::open, errno);
  char chunk[16384];
  bool ok = true;
  for (;;) {
    size_t n = std::fread(chunk, 1, sizeof(chunk), f);
    if (bytes.size() + n > xlsx::MaxArchive) {
      ok = false;
      state.fail(DocumentResult::too_large);
      break;
    }
    bytes.append(chunk, n);
    if (n < sizeof(chunk)) {
      if (std::ferror(f)) {
        ok = false;
        state.fail(DocumentResult::read, errno);
      }
      break;
    }
  }
  if (std::fclose(f) && ok) {
    ok = false;
    state.fail(DocumentResult::close, errno);
  }
  return ok;
}
bool workbook_write(const DocumentIo &io, int fd, const std::string &bytes,
                    TextDocument &state) {
  size_t at = 0;
  while (at < bytes.size()) {
    auto n = io.write(io.context, fd, bytes.data() + at, bytes.size() - at);
    if (n <= 0 || uint64_t(n) > bytes.size() - at)
      return state.fail(DocumentResult::write, n < 0 ? int(-n) : EIO);
    at += size_t(n);
  }
  return true;
}
} // namespace
bool SheetDocument::supports(const char *path) {
  const char *dot = path ? std::strrchr(path, '.') : nullptr;
  if (!dot)
    return false;
  std::string extension = dot;
  for (char &c : extension)
    c = char(std::tolower((unsigned char)c));
  return extension == ".csv" || extension == ".xlsx";
}
bool SheetDocument::load_xlsx(const char *path) {
  if (dirty()) {
    std::snprintf(status, sizeof(status),
                  "Save the current workbook before opening another.");
    return false;
  }
  if (!path || !*path || std::strlen(path) >= sizeof(storage.path))
    return false;
  const std::string name = path;
  std::string bytes, error;
  auto state = new TextDocument;
  if (!workbook_read(name.c_str(), bytes, *state)) {
    std::snprintf(status, sizeof(status),
                  "Could not read XLSX workbook (32 MB archive limit).");
    delete state;
    return false;
  }
  xlsx::Workbook candidate;
  if (!candidate.load(bytes, error)) {
    std::snprintf(status, sizeof(status), "%s",
                  error.empty() ? "Unsupported or damaged XLSX workbook."
                                : error.c_str());
    delete state;
    return false;
  }
  int checked = 0;
  unsigned mode = 0600;
  bool readonly = false;
  if (!matches(name.c_str(), bytes.data(), bytes.size(), false, checked, &mode,
               &readonly)) {
    std::snprintf(status, sizeof(status),
                  "Workbook changed while opening. Try again.");
    delete state;
    return false;
  }
  delete state;
  storage.close(true);
  storage.mode = mode;
  storage.read_only = readonly;
  storage.active = true;
  std::snprintf(storage.path, sizeof(storage.path), "%s", name.c_str());
  workbook = std::move(candidate);
  xlsx_format = true;
  selected_sheet = 0;
  cells = workbook.pages[0].cells;
  cached.fill(false);
  editing = false;
  row = column = first_row = first_column = 0;
  for (const auto &p : workbook.parts)
    if (p.name.find("_xmlsignatures/") == 0)
      storage.read_only = true;
  std::snprintf(
      status, sizeof(status),
      storage.read_only
          ? "XLSX opened read-only."
          : "XLSX opened. Formatting and other workbook parts are preserved.");
  return true;
}
bool SheetDocument::save_xlsx(const DocumentIo *provided) {
  const auto &io = provided ? *provided : document_io();
  auto result = [&](DocumentResult r, int error = 0) {
    storage.fail(r, error);
    std::snprintf(status, sizeof(status), "%s", document_result_message(r));
    return false;
  };
  if (!storage.active || !storage.path[0])
    return result(DocumentResult::invalid_path);
  if (!dirty()) {
    std::snprintf(status, sizeof(status), "Saved");
    return true;
  }
  if (storage.read_only)
    return result(DocumentResult::read_only, EACCES);
  if (!io.create || !io.write || !io.permissions || !io.sync || !io.close ||
      !io.replace || !io.remove)
    return result(DocumentResult::write, EINVAL);
  int checked = 0;
  unsigned mode = storage.mode;
  bool readonly = false;
  const auto &before = workbook.original_bytes;
  if (!matches(storage.path, before.data(), before.size(), false, checked,
               &mode, &readonly))
    return result(checked ? DocumentResult::read : DocumentResult::conflict,
                  checked);
  if (readonly)
    return result(DocumentResult::read_only, EACCES);
  std::string bytes, error;
  std::vector<xlsx::Part> candidate;
  if (!workbook.encode(bytes, candidate, error)) {
    std::snprintf(status, sizeof(status), "%s",
                  error.empty() ? "Workbook could not be encoded."
                                : error.c_str());
    return false;
  }
  char temporary[1120];
  int fd = -1;
  for (int attempt = 0; attempt < 32; ++attempt) {
    std::snprintf(temporary, sizeof(temporary), "%s.wine11-%llx-%x.tmp",
                  storage.path, (unsigned long long)std::time(nullptr),
                  ++save_serial);
    fd = io.create(io.context, temporary, mode);
    if (fd >= 0 || fd != -EEXIST)
      break;
  }
  if (fd < 0)
    return result(DocumentResult::create, -fd);
  auto abandon = [&](DocumentResult r, int code) {
    if (fd >= 0)
      io.close(io.context, fd);
    io.remove(io.context, temporary);
    return result(r, code);
  };
  if (!workbook_write(io, fd, bytes, storage))
    return abandon(storage.result, storage.error);
  int code = io.permissions(io.context, fd, mode);
  if (code < 0)
    return abandon(DocumentResult::permissions, -code);
  code = io.sync(io.context, fd);
  if (code < 0)
    return abandon(DocumentResult::sync, -code);
  int finished = fd;
  fd = -1;
  code = io.close(io.context, finished);
  if (code < 0)
    return abandon(DocumentResult::close, -code);
  if (!matches(temporary, bytes.data(), bytes.size(), false, checked))
    return abandon(DocumentResult::verify, checked ? checked : EIO);
  unsigned final_mode = mode;
  readonly = false;
  if (!matches(storage.path, before.data(), before.size(), false, checked,
               &final_mode, &readonly))
    return abandon(checked ? DocumentResult::read : DocumentResult::conflict,
                   checked);
  if (readonly)
    return abandon(DocumentResult::read_only, EACCES);
  if (final_mode != mode)
    return abandon(DocumentResult::conflict, EAGAIN);
  code = io.replace(io.context, temporary, storage.path);
  if (code < 0)
    return abandon(DocumentResult::replace, -code);
  workbook.committed(std::move(candidate), std::move(bytes));
  storage.mode = mode;
  storage.result = DocumentResult::ok;
  storage.error = 0;
  std::snprintf(status, sizeof(status), "Saved XLSX workbook");
  return true;
}
bool SheetDocument::export_xlsx(const char *path) {
  if (!path || !*path || std::strlen(path) >= sizeof(storage.path))
    return false;
  if (xlsx_format)
    for (const auto &p : workbook.parts)
      if (p.name.find("_xmlsignatures/") == 0) {
        std::snprintf(status, sizeof(status),
                      "Signed workbooks are read-only. Use File Explorer to "
                      "copy the original.");
        return false;
      }
  const int target_sheet = xlsx_format ? selected_sheet : 0;
  auto book = xlsx_format ? workbook : xlsx::Workbook::blank();
  if (!xlsx_format) {
    book.pages[0].cells = cells;
    for (size_t i = 0; i < cells.size(); ++i)
      book.pages[0].formulas[i] = !cells[i].empty() && cells[i][0] == '=';
  }
  std::string bytes, error;
  std::vector<xlsx::Part> parts;
  if (!book.encode(bytes, parts, error)) {
    std::snprintf(status, sizeof(status), "%s", error.c_str());
    return false;
  }
  const auto &io = document_io();
  int fd = io.create(io.context, path, 0600);
  if (fd < 0) {
    storage.error = -fd;
    std::snprintf(status, sizeof(status),
                  "Could not create XLSX copy (choose a new filename).");
    return false;
  }
  bool wrote = workbook_write(io, fd, bytes, storage);
  int synced = io.sync(io.context, fd), closed = io.close(io.context, fd);
  int checked = 0;
  if (!wrote || synced < 0 || closed < 0 ||
      !matches(path, bytes.data(), bytes.size(), false, checked)) {
    io.remove(io.context, path);
    std::snprintf(status, sizeof(status),
                  "XLSX copy could not be written safely.");
    return false;
  }
  book.committed(std::move(parts), std::move(bytes));
  workbook = std::move(book);
  xlsx_format = true;
  selected_sheet = target_sheet;
  storage.close(true);
  storage.active = true;
  storage.read_only = false;
  storage.mode = 0600;
  std::snprintf(storage.path, sizeof(storage.path), "%s", path);
  cached.fill(false);
  std::snprintf(status, sizeof(status),
                "Saved XLSX copy. Original CSV is unchanged.");
  return true;
}
bool SheetDocument::select_sheet(int index) {
  if (!xlsx_format || index < 0 || index >= int(workbook.pages.size()) ||
      editing)
    return false;
  selected_sheet = index;
  cells = workbook.pages[index].cells;
  row = column = first_row = first_column = 0;
  cached.fill(false);
  return true;
}
bool SheetDocument::editable() const {
  if (storage.read_only)
    return false;
  if (cells[row * Columns + column].size() > 255)
    return false;
  for(unsigned char ch:cells[row*Columns+column])
    if(ch<32&&ch!=9&&ch!=10&&ch!=13)return false;
  if (!xlsx_format)
    return true;
  const auto &page = workbook.pages[selected_sheet];
  int pi = workbook.part(page.path);
  xlsx::Xml xml(workbook.parts[pi].bytes);
  std::string error;
  if (!xml.parse(error))
    return false;
  if (xml.child(0, "sheetProtection") >= 0)
    return false;
  for (size_t i = 0; i < xml.nodes.size(); ++i)
    if (xlsx::local_name(xml.nodes[i].name) == "f" &&
        !xml.nodes[i].attr("t").empty()) {
      const auto ref = xml.nodes[i].attr("ref");
      size_t colon = ref.find(':');
      int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
      if (colon != ref.npos && xlsx::coordinate(ref.substr(0, colon), r1, c1) &&
          xlsx::coordinate(ref.substr(colon + 1), r2, c2) && row >= r1 &&
          row <= r2 && column >= c1 && column <= c2)
        return false;
    }
  for (size_t i = 0; i < xml.nodes.size(); ++i)
    if (xlsx::local_name(xml.nodes[i].name) == "c" &&
        xml.nodes[i].attr("r") == xlsx::address(row, column)) {
      int fi = xml.child(int(i), "f");
      return fi < 0 || xml.nodes[fi].attr("t").empty();
    }
  return true;
}
} // namespace ww

namespace ww {
bool SheetDocument::add_sheet() {
  if (!xlsx_format || storage.read_only)
    return false;
  std::string error;
  if (!workbook.add_sheet(error)) {
    std::snprintf(status, sizeof(status), "%s", error.c_str());
    return false;
  }
  editing = false;
  select_sheet(int(workbook.pages.size()) - 1);
  std::snprintf(status, sizeof(status), "New worksheet added");
  return true;
}
} // namespace ww
