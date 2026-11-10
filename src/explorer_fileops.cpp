// SPDX-License-Identifier: GPL-3.0-or-later
#include "explorer_fileops.hpp"
#include "privilege.hpp"
#include "explorer_directory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <chrono>
#include <algorithm>
#include <sys/stat.h>
#include <limits>
#include <string_view>
#ifdef __PROSPERO__
#include <sys/mount.h>
#elif !defined(_WIN32)
#include <sys/statvfs.h>
#ifdef __linux__
#include <mntent.h>
#endif
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif
#ifdef __PROSPERO__
extern "C" {
int sceKernelDlsym(int, const char *, void **);
int sceKernelOpen(const char *, int, int);
int64_t sceKernelRead(int, void *, size_t);
int64_t sceKernelWrite(int, const void *, size_t);
int64_t sceKernelLseek(int, int64_t, int);
int sceKernelFstat(int, struct stat *);
int sceKernelFchmod(int, unsigned);
int sceKernelFsync(int);
int sceKernelClose(int);
int sceKernelGetdents(int, char *, int);
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
  if (error < 0 || (!value.regular && !value.directory) || !value.inode ||
      std::strlen(leaf(source)) > 255) {
    if (error >= 0)
      error = -ENOTSUP;
    return false;
  }
  std::snprintf(path, sizeof(path), "%s", source);
  info = value;
  ready = true;
  move = false;
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
  return begin_transfer(clipboard,folder,provided,false);
}
bool Transfer::begin_recovery(const Clipboard &clipboard, const char *folder,
                              const Io *provided) {
  return begin_transfer(clipboard,folder,provided,true);
}
bool Transfer::begin_transfer(const Clipboard &clipboard, const char *folder,
                              const Io *provided, bool recovery) {
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
  if (initial.size > uint64_t(std::numeric_limits<int64_t>::max()))
    return fail("File size limit", EFBIG);
  if (!recovery && initial.size > MaximumBytes)
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

namespace {
bool join_path(std::string &out, const std::string &base, const std::string &name) {
  out = base + (base.empty() || base.back() != '/' ? "/" : "") + name;
  return out.size() < 1024;
}
int make_directory(const char *path) {
#ifdef _WIN32
  if (CreateDirectoryA(path, nullptr)) return 0;
  return GetLastError() == ERROR_ALREADY_EXISTS ? -EEXIST : -EACCES;
#else
  const int result=ww::privilege::run([&] { return ::mkdir(path, 0700); });
  return result<0 ? -directory::error_or_io(errno) : 0;
#endif
}
bool children(const char *path, std::vector<std::string> &names, size_t limit=4096, bool directories_only=false, size_t skip=0, bool *more=nullptr) {
  size_t seen=0;bool stopped=false;if(more)*more=false;
  auto add = [&](const char *name,bool possibly_directory) {
    if (!std::strcmp(name,".") || !std::strcmp(name,"..")) return true;
    if(directories_only && !possibly_directory && std::strcmp(name,".winbrew-trash"))return true;
    if (std::strchr(name,'/') || std::strchr(name,'\\')) return false;
    if(seen++<skip)return true;
    if(names.size() >= limit){if(more){*more=true;stopped=true;}return false;}
    names.emplace_back(name); return true;
  };
#ifdef _WIN32
  std::string pattern = std::string(path) + "/*";
  WIN32_FIND_DATAA data{}; HANDLE h = FindFirstFileA(pattern.c_str(), &data);
  if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
  bool ok = true;
  do { if (!add(data.cFileName,(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))!=0)) {ok=false;break;} } while (FindNextFileA(h,&data));
  if (ok && GetLastError()!=ERROR_NO_MORE_FILES) ok=false;
  FindClose(h); return ok || stopped;
#elif defined(__PROSPERO__)
  const int fd = error_code(ww::privilege::run([&] {
    return sceKernelOpen(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW,0);
  }));
  if(fd<0)return false;
  // Console /data directories use 64 KiB records; a smaller buffer fails
  // with EINVAL even for a folder containing just one file.
  // Keep this off the caller's stack when scanning deeply nested folders.
  std::vector<unsigned char> buffer(64*1024); bool ok=true;
  while(ok) {
    int n=sceKernelGetdents(fd,reinterpret_cast<char *>(buffer.data()),int(buffer.size()));
    if(n<0 || n>int(buffer.size())){ok=false;break;} if(!n)break;
    size_t at=0; directory::Record item{};
    while(true) {
      auto r=directory::next(buffer.data(),size_t(n),at,item);
      if(r==directory::Read::end)break;
      if(r==directory::Read::invalid){ok=false;break;}
      if(item.inode && !add(item.name,item.type==DT_DIR || item.type==DT_UNKNOWN || item.type==DT_LNK)){ok=false;break;}
    }
  }
  if(close_file(nullptr,fd)<0)return false;
  return ok || stopped;
#else
  DIR *d=opendir(path); if(!d)return false; bool ok=true;
  while(true){errno=0;auto *entry=readdir(d);if(!entry){if(errno)ok=false;break;}if(!add(entry->d_name,entry->d_type==DT_DIR || entry->d_type==DT_UNKNOWN || entry->d_type==DT_LNK)){ok=false;break;}}
  if(closedir(d)!=0)return false;
  return ok || stopped;
#endif
}
bool tree_append(std::vector<TreeTransfer::Entry> &entries,const std::string &path,const Info &info){entries.push_back({path,info});return true;}
bool tree_append(CheckedTransferEntries &entries,const std::string &path,const Info &info){return entries.append(path.c_str(),info);}
template<class List>
bool scan_tree(const std::string &root, const std::string &relative,
               List &entries, unsigned depth=0,
               bool recovery=false) {
  constexpr bool checked=std::is_same_v<List,CheckedTransferEntries>;
  if(depth>(recovery?64u:32u) || ((!recovery || !checked) && entries.size()>=(recovery?65536u:4096u)))return false;
  std::string path=root;
  if(!relative.empty() && !join_path(path,root,relative))return false;
  Info info{}; if(inspect(path.c_str(),info)<0 || (!info.regular&&!info.directory))return false;
  if(!tree_append(entries,relative,info))return false;
  if(info.directory){
    size_t offset=0;bool more=false;
    do {
      std::vector<std::string> names;
      if(recovery && checked){if(!children(path.c_str(),names,512,false,offset,&more))return false;offset+=names.size();}
      else if(!children(path.c_str(),names,recovery?65536:4096))return false;
      std::sort(names.begin(),names.end());
      for(const auto &name:names){
        // Recovery stores are never recursively copied into new documents.
        if(!recovery && name==".winbrew-trash")continue;
        std::string next=relative.empty()?name:relative+"/"+name;
        if(!scan_tree(root,next,entries,depth+1,recovery))return false;
      }
      Info page_after{};if(inspect(path.c_str(),page_after)<0 || !unchanged(info,page_after))return false;
    }while(more);
  }
  Info after{};return inspect(path.c_str(),after)>=0 && unchanged(info,after);
}
template<class A,class B>
bool same_tree(const A &a,const B &b){
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i)if(std::string_view(a[i].relative)!=std::string_view(b[i].relative) || !unchanged(a[i].info,b[i].info))return false;
  return true;
}
}
bool RecoveryWalker::begin(const std::vector<std::string> &roots,const char *exclude) {
  pending.clear();stores.clear();names.clear();current.clear();excluded=exclude?exclude:"";
  current_device=0;name_index=name_offset=directories=0;names_more=false;active=complete=failed=false;
  std::vector<Pending> checked;
  for(auto path:roots){
    while(path.size()>1 && path.back()=='/')path.pop_back();
    Info info{};if(path.empty() || path.size()>=1024 || inspect(path.c_str(),info)<0 || !info.directory){failed=true;return false;}
    checked.push_back({std::move(path),info.device,info.inode});
  }
  std::sort(checked.begin(),checked.end(),[](const Pending &a,const Pending &b){return a.path.size()==b.path.size()?a.path<b.path:a.path.size()<b.path.size();});
  for(auto &candidate:checked){
    bool covered=false;
    for(const auto &existing:pending)if(existing.device==candidate.device &&
       (candidate.path==existing.path || (existing.path=="/"?candidate.path.front()=='/':candidate.path.compare(0,existing.path.size()+1,existing.path+"/")==0))){covered=true;break;}
    if(!covered)pending.push_back(std::move(candidate));
  }
  if(pending.empty()){complete=true;return true;}
  active=true;return true;
}
void RecoveryWalker::cancel(){active=false;complete=false;pending.clear();names.clear();current.clear();stores.clear();}
void RecoveryWalker::step(size_t budget) {
  if(!active || !budget)return;
  auto failure=[&]{active=false;failed=true;complete=false;pending.clear();names.clear();current.clear();stores.clear();};
  while(active && budget){
    if(current.empty()){
      if(pending.empty()){std::sort(stores.begin(),stores.end());stores.erase(std::unique(stores.begin(),stores.end()),stores.end());active=false;complete=true;return;}
      auto next=std::move(pending.back());pending.pop_back();current=std::move(next.path);current_device=next.device;
      if(inspect(current.c_str(),current_info)<0 || !current_info.directory || current_info.device!=current_device || current_info.inode!=next.inode){failure();return;}
      names.clear();name_index=name_offset=0;names_more=false;
      if(!children(current.c_str(),names,1024,true,0,&names_more)){failure();return;}
      ++directories;--budget;
    }
    while(budget && name_index<names.size()){
      const auto name=names[name_index++];--budget;std::string path;
      if(!join_path(path,current,name) || path.size()>=1024){failure();return;}
#ifndef _WIN32
      // Only directories can contain an old store. Inspect the directory
      // entry without opening regular, private or special files.
      struct stat entry{};
      if(ww::privilege::run([&]{return ::lstat(path.c_str(),&entry);})!=0){failure();return;}
      if(!S_ISDIR(entry.st_mode)){if(name==".winbrew-trash"){failure();return;}continue;}
#endif
      Info info{};const int code=inspect(path.c_str(),info);
      if(code<0){if(code==-ELOOP && name!=".winbrew-trash")continue;failure();return;}
      if(!info.directory){if(name==".winbrew-trash"){failure();return;}continue;}
      if(info.device!=current_device)continue;
      if(name==".winbrew-trash"){if(path!=excluded)stores.push_back(std::move(path));}
      else pending.push_back({std::move(path),current_device,info.inode});
    }
    if(name_index==names.size()){
      Info after{};if(inspect(current.c_str(),after)<0 || !unchanged(current_info,after)){failure();return;}
      if(names_more){
        name_offset+=names.size();names.clear();name_index=0;
        if(!children(current.c_str(),names,1024,true,name_offset,&names_more)){failure();return;}
        continue;
      }
      current.clear();names.clear();name_index=0;
    }
  }
}
bool trash(const char *path, const Info &expected, char *recovery, size_t capacity, const char *store_path) {
  if(!path || !*path)return false;
  std::string source=path; while(source.size()>1 && source.back()=='/')source.pop_back();
  auto slash=source.find_last_of("/\\");
  if(slash==std::string::npos || source.substr(slash+1).empty() || source.substr(slash+1)==".winbrew-trash" || source.substr(slash+1)==".." || source.substr(slash+1)==".")return false;
  Info actual{};if(inspect(source.c_str(),actual)<0 || !unchanged(expected,actual))return false;
  std::string base=store_path && *store_path ? store_path : source.substr(0,slash)+"/.winbrew-trash";
  if(base.size()<15 || base.compare(base.size()-15,15,"/.winbrew-trash")!=0)return false;
  int code=make_directory(base.c_str()); if(code<0 && code!=-EEXIST)return false;
  Info store{}; if(inspect(base.c_str(),store)<0 || !store.directory || store.device!=actual.device)return false;
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  for(unsigned i=0;i<100;++i){
    const std::string label=store_path && *store_path ? source.substr(slash+1,170)+" - " : "";
    std::string box=base+"/"+label+std::to_string(stamp)+"-"+std::to_string(i);
    std::string target=box+"/"+source.substr(slash+1);
    if(target.size()>=capacity)return false;
    code=make_directory(box.c_str());if(code==-EEXIST)continue;if(code<0)return false;
    auto remove_empty=[&]{
#ifdef _WIN32
      RemoveDirectoryA(box.c_str());
#else
      ww::privilege::run([&]{return ::rmdir(box.c_str());});
#endif
    };
    if(inspect(source.c_str(),actual)<0 || !unchanged(expected,actual)){remove_empty();return false;}
    if(ww::privilege::run([&]{return std::rename(source.c_str(),target.c_str());})!=0){remove_empty();return false;}
    Info moved{};if(inspect(target.c_str(),moved)<0 || !identity(expected,moved))return false;
    std::snprintf(recovery,capacity,"%s",target.c_str());return true;
  }
  return false;
}
bool TreeTransfer::fail(const char *where,int code){
  if(child && child->busy())child->cancel();
  state=State::failed;error=code<0?-code:code;std::snprintf(stage,sizeof(stage),"%s",where);return false;
}
bool TreeTransfer::begin(const Clipboard &clip,const char *folder,bool recovery,const char *store){
  include_recovery=recovery;
  if(busy())return false;
  child.reset();entries.clear();outputs.clear();index=0;copied=verified=total=0;error=0;destination_created=false;moving=clip.move;
  state=State::idle;destination[0]=0;
  if(!clip.ready || !folder || std::strlen(folder)>=1024)return fail("Clipboard",EINVAL);
  if(store && std::strlen(store)>=sizeof(recovery_store))return fail("Recycle Bin path",ENAMETOOLONG);
  std::snprintf(recovery_store,sizeof(recovery_store),"%s",store?store:"");
  std::snprintf(source,sizeof(source),"%s",clip.path);std::snprintf(directory,sizeof(directory),"%s",folder);
  Info now{},parent{};
  if(inspect(source,now)<0 || !unchanged(now,clip.info))return fail("Source changed",EAGAIN);
  if(inspect(folder,parent)<0 || !parent.directory)return fail("Destination folder",ENOTDIR);
  // Reject a descendant destination by identity, including alternate spellings.
  std::string ancestor=folder;
  while(!ancestor.empty()){
    Info p{};if(inspect(ancestor.c_str(),p)<0)return fail("Destination path",EACCES);
    if(identity(p,now))return fail("Folder inside itself",EINVAL);
    auto at=ancestor.find_last_of("/\\");if(at==std::string::npos || !at)break;ancestor.resize(at);
  }
  if(!scan_tree(source,"",entries,0,include_recovery))return fail("Scan source",ENOTSUP);
  for(const auto &e:entries)if(e.info.regular){if(e.info.size>uint64_t(std::numeric_limits<int64_t>::max()) || (!include_recovery&&e.info.size>Transfer::MaximumBytes) || e.info.size>std::numeric_limits<uint64_t>::max()-total)return fail("File size limit",EFBIG);total+=e.info.size;}
  if(now.directory){
    int code=-EEXIST;
    for(int n=0;n<100 && code==-EEXIST;++n){
      if(!destination_name(destination,sizeof(destination),folder,leaf(source),n))return fail("Destination path",ENAMETOOLONG);
      code=make_directory(destination);
    }
    if(code<0)return fail("Create folder",code);
    Info created{};if(inspect(destination,created)<0 || !created.directory)return fail("Created folder",EIO);
    if(!outputs.append(destination,created))return fail("Snapshot memory",ENOMEM);
    destination_created=true;index=1;
  }
  state=State::copying;std::snprintf(stage,sizeof(stage),"Preparing copy");return true;
}
void TreeTransfer::cancel(){if(child)child->cancel();state=State::cancelled;}
void TreeTransfer::step(){
  if(!busy())return;
  if(child){
    const auto oldcopy=child->copied, oldverify=child->verified;
    child->step();copied+=child->copied-oldcopy;verified+=child->verified-oldverify;
    state=child->state==State::verifying?State::verifying:State::copying;
    if(child->state==State::failed){fail(child->stage,child->error);return;}
    if(child->state!=State::complete)return;
    if(entries.size()==1 && entries[0].info.regular)std::snprintf(destination,sizeof(destination),"%s",child->destination);
    Info written{};if(inspect(child->destination,written)<0 || !identity(child->created,written) || written.size!=child->total){fail("Destination changed",EAGAIN);return;}
    if(!outputs.append(child->destination,written)){fail("Snapshot memory",ENOMEM);return;}
    child.reset();++index;return;
  }
  if(index<entries.size()){
    const auto &e=entries[index];std::string src=source,dest=destination;
    if(*e.relative && (!join_path(src,source,e.relative)||!join_path(dest,destination,e.relative))){fail("Path too long",ENAMETOOLONG);return;}
    if(e.info.directory){int code=make_directory(dest.c_str());if(code<0){fail("Create subfolder",code);return;}Info created{};if(inspect(dest.c_str(),created)<0 || !created.directory){fail("Created subfolder",EIO);return;}if(!outputs.append(dest.c_str(),created)){fail("Snapshot memory",ENOMEM);return;}++index;return;}
    Clipboard clip;clip.ready=true;clip.info=e.info;std::snprintf(clip.path,sizeof(clip.path),"%s",src.c_str());
    std::string parent=index==0?directory:dest.substr(0,dest.find_last_of('/'));
    child=std::make_unique<Transfer>();
    const bool opened=include_recovery?child->begin_recovery(clip,parent.c_str()):child->begin(clip,parent.c_str());
    if(!opened){fail(child->stage,child->error);return;}
    if(index!=0 && dest!=child->destination){fail("Destination collision",EEXIST);return;}
    destination_created=true;
    if(index==0 && entries[0].info.regular)std::snprintf(destination,sizeof(destination),"%s",child->destination);
    return;
  }
  for(const auto &entry:outputs){Info actual{};if(inspect(entry.relative,actual)<0 || !(entry.info.directory?identity(entry.info,actual):unchanged(entry.info,actual))){fail("Destination changed",EAGAIN);return;}}
  CheckedTransferEntries current;
  if(!scan_tree(source,"",current,0,include_recovery)||!same_tree(entries,current)){fail("Source changed",EAGAIN);return;}
  if(moving){char recovery[1024];if(!trash(source,entries[0].info,recovery,sizeof(recovery),recovery_store[0]?recovery_store:nullptr)){fail("Move source retained",EACCES);return;}}
  state=State::complete;
}

bool recovery_location(const char *folder, char *out, size_t capacity) {
  if (!folder || !*folder) return false;
  std::string path(folder);
  while (path.size()>1 && path.back()=='/') path.pop_back();
  const std::string marker="/.winbrew-trash";
  size_t at=0;
  while ((at=path.find(marker,at))!=std::string::npos) {
    const auto end=at+marker.size();
    if(end==path.size() || path[end]=='/') {path.resize(end);break;}
    at=end;
  }
  if(at==std::string::npos) path+=marker;
  if(path.size()>=capacity)return false;
  std::snprintf(out,capacity,"%s",path.c_str());return true;
}
bool legacy_recovery_name(const char *path, std::string &name) {
  if(!path || !*path)return false;
  const std::string value(path);const auto slash=value.find_last_of('/');
  if(slash==std::string::npos)return false;
  const auto leaf=value.substr(slash+1);const auto dash=leaf.find('-');
  if(dash==std::string::npos || dash==0 || dash+1==leaf.size())return false;
  for(size_t i=0;i<leaf.size();++i)if(i!=dash && (leaf[i]<'0'||leaf[i]>'9'))return false;
  Info info{};if(inspect(path,info)<0 || !info.directory)return false;
  std::vector<std::string> names;if(!children(path,names) || names.size()!=1)return false;
  name=names.front();return true;
}
bool discover_recovery_stores(const char *root_path, std::vector<std::string> &output) {
  if(!root_path || !*root_path || std::strlen(root_path)>=1024)return false;
  Info root{};if(inspect(root_path,root)<0 || !root.directory)return false;
  struct Pending { std::string path; unsigned depth; };
  std::vector<Pending> pending{{root_path,0}};std::vector<std::string> result;size_t visited=0;
  while(!pending.empty()) {
    auto item=std::move(pending.back());pending.pop_back();
    if(++visited>4096 || item.depth>32)return false;
    Info before{};if(inspect(item.path.c_str(),before)<0 || !before.directory || before.device!=root.device)return false;
    std::vector<std::string> names;if(!children(item.path.c_str(),names))return false;
    for(const auto &name:names) {
      std::string path;if(!join_path(path,item.path,name) || path.size()>=1024)return false;
      Info info{};const int rc=inspect(path.c_str(),info);
      if(rc<0){if(rc==-ELOOP && name!=".winbrew-trash")continue;return false;}
      if(!info.directory){if(name==".winbrew-trash")return false;continue;}
      if(info.device!=root.device)return false;
      if(name==".winbrew-trash") {if(result.size()>=512)return false;result.push_back(std::move(path));}
      else pending.push_back({std::move(path),item.depth+1});
    }
    Info after{};if(inspect(item.path.c_str(),after)<0 || !unchanged(before,after))return false;
  }
  std::sort(result.begin(),result.end());output=std::move(result);return true;
}
bool collect_recovery_batch(const std::vector<std::string> &stores,const char *destination,std::vector<Clipboard> &output,bool &more) {
  constexpr size_t batch=256, page=512;
  if(!destination)return false;
  const std::string dest(destination);
  if(dest.size()<15 || dest.compare(dest.size()-15,15,"/.winbrew-trash")!=0)return false;
  std::vector<Clipboard> result;bool full=false;
  for(const auto &store:stores){
    if(store==dest)continue;
    Info store_before{};if(inspect(store.c_str(),store_before)<0 || !store_before.directory)return false;
    size_t offset=0;bool next=false;
    do {
      std::vector<std::string> names;if(!children(store.c_str(),names,page,false,offset,&next))return false;
      offset+=names.size();
      for(const auto &name:names){
        std::string container;if(!join_path(container,store,name))return false;
        Info before{};if(inspect(container.c_str(),before)<0 || (!before.directory&&!before.regular))return false;
        if(before.directory){
          size_t inner_offset=0;bool inner_next=false;
          do {
            std::vector<std::string> items;if(!children(container.c_str(),items,page,false,inner_offset,&inner_next))return false;
            inner_offset+=items.size();
            for(const auto &item:items){
              std::string path;Clipboard clip;if(!join_path(path,container,item) || !clip.copy(path.c_str()))return false;
              result.push_back(clip);if(result.size()==batch){full=true;break;}
            }
            Info after{};if(inspect(container.c_str(),after)<0 || !unchanged(before,after))return false;
          }while(inner_next&&!full);
        }else{
          Clipboard clip;if(!clip.copy(container.c_str()))return false;
          result.push_back(clip);if(result.size()==batch)full=true;
        }
        if(full)break;
      }
      Info after{};if(inspect(store.c_str(),after)<0 || !unchanged(store_before,after))return false;
    }while(next&&!full);
    if(full)break;
  }
  output=std::move(result);more=full;return true;
}
bool collect_recovery_stores(const std::vector<std::string> &stores,const char *destination,std::vector<Clipboard> &output) {
  if(!destination)return false;
  const std::string dest(destination);
  if(dest.size()<15 || dest.compare(dest.size()-15,15,"/.winbrew-trash")!=0)return false;
  std::vector<Clipboard> result;
  for(const auto &store:stores){
    if(store==dest)continue;
    std::vector<std::string> names;if(!children(store.c_str(),names,65536))return false;
    std::sort(names.begin(),names.end());
    for(const auto &name:names){
      std::string container;if(!join_path(container,store,name))return false;
      Info info{};if(inspect(container.c_str(),info)<0 || (!info.directory&&!info.regular))return false;
      if(info.directory){
        std::vector<std::string> items;if(!children(container.c_str(),items,65536))return false;
        std::sort(items.begin(),items.end());
        for(const auto &item:items){std::string path;Clipboard clip;if(!join_path(path,container,item) || !clip.copy(path.c_str()))return false;result.push_back(clip);}
      } else {Clipboard clip;if(!clip.copy(container.c_str()))return false;result.push_back(clip);}
      if(result.size()>65536)return false;
    }
  }
  output=std::move(result);return true;
}
bool collect_recovery_items(const char *root,const char *destination,std::vector<Clipboard> &output) {
  if(!root)return false;std::vector<std::string> stores;
  return discover_recovery_stores(root,stores) && collect_recovery_stores(stores,destination,output);
}
bool consolidate_recovery_stores(const char *profile,const char *destination,size_t &moved) {
  moved=0;if(!profile||!destination)return false;
  std::vector<std::string> stores;if(!discover_recovery_stores(profile,stores))return false;
  const std::string dest(destination);
  if(dest.size()<15 || dest.compare(dest.size()-15,15,"/.winbrew-trash")!=0)return false;
  for(const auto &store:stores) {
    if(store==dest)continue;
    std::vector<std::string> names;if(!children(store.c_str(),names))return false;
    std::sort(names.begin(),names.end());
    for(const auto &name:names) {
      std::string source;if(!join_path(source,store,name) || source.size()>=1024)return false;
      Info info{};char result[1024];
      if(inspect(source.c_str(),info)<0 || (!info.regular&&!info.directory))return false;
      // Older stores wrap each deleted item in a generated directory. Move its
      // contents, so the central bin shows the original names, not wrapper IDs.
      if(info.directory) {
        std::vector<std::string> items;if(!children(source.c_str(),items))return false;
        std::sort(items.begin(),items.end());
        for(const auto &item:items) {
          std::string original;if(!join_path(original,source,item) || original.size()>=1024)return false;
          Info entry{};if(inspect(original.c_str(),entry)<0 || (!entry.regular&&!entry.directory) || !trash(original.c_str(),entry,result,sizeof(result),dest.c_str()))return false;
          ++moved;
        }
      } else {
        if(!trash(source.c_str(),info,result,sizeof(result),dest.c_str()))return false;
        ++moved;
      }
    }
  }
  return true;
}
namespace {
#ifndef _WIN32
int anchored_directory(const char *path, int anchor=-1) {
  if(!path || !*path)return -EINVAL;
  int fd=ww::privilege::run([&] {
    return anchor<0 ? ::open(*path=='/'?"/":".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW)
                    : ::openat(anchor,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
  });
  if(fd<0)return -directory::error_or_io(errno);
  const char *at=path+(*path=='/'?1:0);
  while(*at) {
    const char *end=std::strchr(at,'/');if(!end)end=at+std::strlen(at);
    const size_t length=size_t(end-at);if(!length){at=end+1;continue;}
    if(length>=1024 || (length==1 && at[0]=='.') || (length==2 && at[0]=='.' && at[1]=='.')){close_file(nullptr,fd);return -EINVAL;}
    char part[1024];std::memcpy(part,at,length);part[length]=0;at=*end?end+1:end;
    int next=ww::privilege::run([&]{return ::openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);});
    int error=errno;close_file(nullptr,fd);
    if(next<0)return -directory::error_or_io(error);
    fd=next;
  }
  return fd;
}
int anchored_directory(const std::string &path,int anchor=-1){return anchored_directory(path.c_str(),anchor);}
#endif
}
void CrossVolumeRecycle::close() {
  if(parent_fd>=0)close_file(nullptr,parent_fd);
  if(store_fd>=0)close_file(nullptr,store_fd);
  parent_fd=store_fd=-1;
}
bool CrossVolumeRecycle::fail(int code) {
  error=code<0?-code:code;state=State::failed;close();return false;
}
bool CrossVolumeRecycle::begin(const Clipboard &clip,const char *store) {
  if(busy())return false;
  close();state=State::idle;removing=false;remaining=removed=0;error=0;parent.clear();leaf.clear();store_path.clear();
  if(!clip.ready || !store || !*store)return fail(EINVAL);
  const std::string bin(store),source(clip.path);
  if(bin.size()<15 || bin.compare(bin.size()-15,15,"/.winbrew-trash")!=0 ||
     source.compare(0,bin.size()+1,bin+"/")==0)return fail(EINVAL);
  const auto slash=source.find_last_of('/');
  if(slash==std::string::npos || slash+1==source.size())return fail(EINVAL);
  parent=slash==0?"/":source.substr(0,slash);leaf=source.substr(slash+1);
  if(leaf=="." || leaf==".." || leaf.find('\\')!=std::string::npos)return fail(EINVAL);
  Info current{};
  if(inspect(source.c_str(),current)<0 || !unchanged(current,clip.info) ||
     inspect(bin.c_str(),store_info)<0 || !store_info.directory)return fail(EAGAIN);
  store_path=bin;
#ifdef _WIN32
  for(size_t i=0;i<=bin.size();++i)if(i==bin.size() || (i>2 && bin[i]=='/')){Info ancestor{};if(inspect(bin.substr(0,i).c_str(),ancestor)<0 || !ancestor.directory)return fail(EACCES);}
  store_fd=open_file(nullptr,bin.c_str(),false);
#else
  store_fd=anchored_directory(bin);
#endif
  Info held_store{};
  if(store_fd<0 || metadata(nullptr,store_fd,held_store)<0 || !identity(store_info,held_store))return fail(EACCES);
  Clipboard copy=clip;copy.move=false;
  if(!transfer.begin(copy,store,true))return fail(transfer.error?transfer.error:EIO);
  state=State::copying;return true;
}
void CrossVolumeRecycle::cancel() {
  if(transfer.busy())transfer.cancel();state=State::cancelled;close();
}
void CrossVolumeRecycle::step() {
  if(!busy())return;
  Info named_store{},held_store{};
  if(inspect(store_path.c_str(),named_store)<0 || metadata(nullptr,store_fd,held_store)<0 ||
     !identity(store_info,named_store) || !identity(store_info,held_store)){
    if(transfer.busy())transfer.cancel();fail(EAGAIN);return;
  }
  if(!removing) {
    if(transfer.busy())transfer.step();
    if(transfer.busy()){state=transfer.state;return;}
    if(transfer.state!=State::complete){fail(transfer.error?transfer.error:EIO);return;}
    CheckedTransferEntries current;
    if(transfer.entries.empty() || transfer.entries.size()!=transfer.outputs.size() ||
       !scan_tree(transfer.source,"",current,0,true) || !same_tree(transfer.entries,current)) {fail(EAGAIN);return;}
    for(const auto &out:transfer.outputs){Info item{};if(inspect(out.relative,item)<0 || !(out.info.directory?identity(out.info,item):unchanged(out.info,item))){fail(EAGAIN);return;}}
#ifndef _WIN32
    // The copied file contents are already flushed by Transfer. Flush the
    // destination directory entries too, before removing any source data.
    for(auto it=transfer.outputs.rbegin();it!=transfer.outputs.rend();++it)if(it->info.directory){
      int fd=anchored_directory(it->relative);if(fd<0){fail(EACCES);return;}
      const int result=ww::privilege::run([&]{return ::fsync(fd);});close_file(nullptr,fd);
      if(result!=0){fail(EIO);return;}
    }
    if(ww::privilege::run([&]{return ::fsync(store_fd);})!=0){fail(EIO);return;}
    // If the bin itself was just created, persist its entry in the parent too.
    std::string bin_parent=store_path.substr(0,store_path.size()-15);
    if(bin_parent.empty())bin_parent="/";
    int bin_parent_fd=anchored_directory(bin_parent);if(bin_parent_fd<0){fail(EACCES);return;}
    const int synced=ww::privilege::run([&]{return ::fsync(bin_parent_fd);});close_file(nullptr,bin_parent_fd);
    if(synced!=0){fail(EIO);return;}
#endif
#ifdef _WIN32
    Info named{};if(inspect(parent.c_str(),named)<0 || !named.directory){fail(EACCES);return;}
    parent_fd=open_file(nullptr,parent.c_str(),false);
#else
    parent_fd=anchored_directory(parent);
#endif
    if(parent_fd<0){parent_fd=-1;fail(EACCES);return;}
    if(metadata(nullptr,parent_fd,parent_info)<0 || !parent_info.directory){fail(EACCES);return;}
    remaining=transfer.entries.size();removing=true;state=State::copying;return;
  }
  if(!remaining){state=State::complete;close();return;}
  Info named_parent{},held{};
  if(inspect(parent.c_str(),named_parent)<0 || metadata(nullptr,parent_fd,held)<0 ||
     !identity(parent_info,named_parent) || !identity(parent_info,held)){fail(EAGAIN);return;}
  const size_t index=remaining-1;const auto &entry=transfer.entries[index];const auto &out=transfer.outputs[index];
  Info copied{};if(inspect(out.relative,copied)<0 || !(out.info.directory?identity(out.info,copied):unchanged(out.info,copied))){fail(EAGAIN);return;}
  const std::string relative=!*entry.relative?leaf:leaf+"/"+entry.relative;
#ifdef _WIN32
  const std::string path=parent+"/"+relative;
  HANDLE handle=CreateFileA(path.c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
  if(handle==INVALID_HANDLE_VALUE){fail(EACCES);return;}
  int fd=_open_osfhandle(reinterpret_cast<intptr_t>(handle),_O_BINARY|_O_RDONLY);
  if(fd<0){CloseHandle(handle);fail(EACCES);return;}
  Info actual{};const bool matches=metadata(nullptr,fd,actual)>=0 && (entry.info.directory?identity(entry.info,actual):unchanged(entry.info,actual));
  FILE_DISPOSITION_INFO disposition{};disposition.DeleteFile=TRUE;
  const bool erased=matches && SetFileInformationByHandle(handle,FileDispositionInfo,&disposition,sizeof(disposition));
  close_file(nullptr,fd);if(!erased){fail(matches?EACCES:EAGAIN);return;}
#else
  const auto slash=relative.find_last_of('/');const auto name=slash==std::string::npos?relative:relative.substr(slash+1);
  const auto directory=slash==std::string::npos?std::string():relative.substr(0,slash);
  int dir=directory.empty()?ww::privilege::run([&]{return ::openat(parent_fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);}):anchored_directory(directory,parent_fd);
  if(dir<0){fail(EACCES);return;}
  int target=ww::privilege::run([&]{return ::openat(dir,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|(entry.info.directory?O_DIRECTORY:0));});
  Info actual{};const bool matches=target>=0 && metadata(nullptr,target,actual)>=0 && (entry.info.directory?identity(entry.info,actual):unchanged(entry.info,actual));
  bool erased=false;if(matches)erased=ww::privilege::run([&]{return ::unlinkat(dir,name.c_str(),entry.info.directory?AT_REMOVEDIR:0);})==0;
  if(target>=0)close_file(nullptr,target);if(erased)ww::privilege::run([&]{return ::fsync(dir);});close_file(nullptr,dir);
  if(!erased){fail(matches?EACCES:EAGAIN);return;}
#endif
  --remaining;++removed;if(!remaining){state=State::complete;close();}
}
void Purge::close() {
  if(root_fd>=0)close_file(nullptr,root_fd);
  root_fd=-1;
}
bool Purge::fail(int code) {
  error=code<0?-code:code;ready=false;state=State::failed;close();entries.clear();return false;
}
namespace {
bool recovery_ancestors(const std::string &root,const char *relative) {
  if(!relative)return false;
  for(const char *slash=std::strchr(relative,'/');slash;slash=std::strchr(slash+1,'/')){
    const size_t part=size_t(slash-relative);char path[1024];
    if(root.size()+1+part>=sizeof(path))return false;
    std::memcpy(path,root.data(),root.size());path[root.size()]='/';
    std::memcpy(path+root.size()+1,relative,part);path[root.size()+1+part]=0;Info info{};
    if(inspect(path,info)<0 || !info.directory)return false;
  }
  return true;
}
// Stream directory entries into a checked in-memory snapshot. Unlike a vector
// of every name in a directory, this has no fixed item-count limit and can
// report allocation failure before permanently deleting anything.
bool recovery_snapshot_tree(char *path,size_t root_length,RecoverySnapshot<Info> &entries,uint64_t &bytes,bool root) {
  const size_t length=std::strlen(path);Info before{};
  if(length>=1024 || inspect(path,before)<0 || (!before.regular&&!before.directory))return false;
  if(!root){
    if(length<=root_length || path[root_length]!='/')return false;
    if(!entries.append(path+root_length+1,before))return false;
    if(before.regular){if(before.size>std::numeric_limits<uint64_t>::max()-bytes)return false;bytes+=before.size;}
  }
  if(before.directory){
    auto child=[&](const char *name){
      if(!std::strcmp(name,".")||!std::strcmp(name,".."))return true;
      const size_t count=std::strlen(name);if(!count || std::strchr(name,'/') || std::strchr(name,'\\'))return false;
      const bool slash=length && path[length-1]!='/';
      if(length+size_t(slash)+count>=1024)return false;
      if(slash)path[length]='/';std::memcpy(path+length+size_t(slash),name,count+1);
      const bool ok=recovery_snapshot_tree(path,root_length,entries,bytes,false);path[length]=0;return ok;
    };
    bool ok=true;
#ifdef _WIN32
    char pattern[1028];const int n=std::snprintf(pattern,sizeof(pattern),"%s/*",path);
    if(n<0 || size_t(n)>=sizeof(pattern))return false;
    WIN32_FIND_DATAA data{};HANDLE handle=FindFirstFileA(pattern,&data);
    if(handle==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_NOT_FOUND)return false;}
    else {do{if(!child(data.cFileName)){ok=false;break;}}while(FindNextFileA(handle,&data));if(ok && GetLastError()!=ERROR_NO_MORE_FILES)ok=false;FindClose(handle);}
#elif defined(__PROSPERO__)
    const int fd=error_code(ww::privilege::run([&]{return sceKernelOpen(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW,0);}));
    if(fd<0)return false;
    auto *buffer=static_cast<unsigned char *>(std::malloc(64*1024));
    if(!buffer){close_file(nullptr,fd);return false;}
    while(ok){
      const int n=sceKernelGetdents(fd,reinterpret_cast<char *>(buffer),64*1024);
      if(n<0 || n>64*1024){ok=false;break;}if(!n)break;
      size_t at=0;directory::Record item{};
      while(true){const auto state=directory::next(buffer,size_t(n),at,item);if(state==directory::Read::end)break;if(state==directory::Read::invalid){ok=false;break;}if(item.inode && !child(item.name)){ok=false;break;}}
    }
    std::free(buffer);if(close_file(nullptr,fd)<0)ok=false;
#else
    DIR *directory=nullptr;ww::privilege::run([&]{directory=opendir(path);return directory?0:-1;});
    if(!directory)return false;
    while(true){errno=0;auto *item=readdir(directory);if(!item){if(errno)ok=false;break;}if(!child(item->d_name)){ok=false;break;}}
    if(closedir(directory)!=0)ok=false;
#endif
    if(!ok)return false;
  }
  Info after{};return inspect(path,after)>=0 && unchanged(before,after);
}
}
bool Purge::prepare(const char *path) {
  return prepare_target(path,nullptr);
}
bool Purge::prepare_target(const char *path,const char *relative) {
  if(busy())return false;
  close();entries.clear();selected.clear();remaining=removed=0;bytes=0;error=0;ready=false;state=State::idle;
  if(!path || std::strlen(path)>=1024)return fail(EINVAL);
  root=path;while(root.size()>1 && root.back()=='/')root.pop_back();
  const auto slash=root.find_last_of('/');
  if(slash==std::string::npos || root.substr(slash+1)!=".winbrew-trash")return fail(EINVAL);
#ifdef _WIN32
  // Reject links/reparse points in every ancestor before opening the store.
  for(size_t i=0;i<=root.size();++i)if(i==root.size() || (i>2 && root[i]=='/')){
    Info ancestor{};const int code=inspect(root.substr(0,i).c_str(),ancestor);
    if(code==-ENOENT && i==root.size()){state=State::complete;return true;}
    if(code<0 || !ancestor.directory)return fail(EACCES);
  }
  root_fd=open_file(nullptr,root.c_str(),false);
#else
  root_fd=anchored_directory(root);
#endif
  if(root_fd<0){const int code=root_fd;root_fd=-1;if(code==-ENOENT){state=State::complete;return true;}return fail(code);}
  if(metadata(nullptr,root_fd,root_info)<0 || !root_info.directory)return fail(EACCES);
  Info named{};
  if(inspect(root.c_str(),named)<0 || !identity(root_info,named))return fail(EAGAIN);
  if(!unchanged(root_info,named))return fail(EAGAIN);
  char path_buffer[1024];
  if(relative){selected=relative;if(!recovery_ancestors(root,selected.c_str()))return fail(ENOTSUP);}
  const int length=std::snprintf(path_buffer,sizeof(path_buffer),relative?"%s/%s":"%s",root.c_str(),relative?relative:"");
  if(length<0 || size_t(length)>=sizeof(path_buffer) || !recovery_snapshot_tree(path_buffer,root.size(),entries,bytes,!relative))return fail(ENOTSUP);
  Info after{};if(inspect(root.c_str(),after)<0 || !unchanged(named,after))return fail(EAGAIN);
  remaining=entries.size();
  ready=remaining>0;
  if(!ready){state=State::complete;close();}
  return true;
}
bool Purge::prepare_item(const char *store,const char *item) {
  if(!store||!item)return false;
  const std::string root_path(store),item_path(item),prefix=root_path+"/";
  if(item_path.compare(0,prefix.size(),prefix)!=0)return false;
  const std::string relative=item_path.substr(prefix.size());
  if(relative.empty() || relative.find('\\')!=std::string::npos)return false;
  for(size_t at=0;at<relative.size();) {auto end=relative.find('/',at);if(end==std::string::npos)end=relative.size();const auto part=relative.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;if(end+1==relative.size())return false;}
  return prepare_target(store,relative.c_str()) && ready;
}
bool Purge::begin() {
  if(!ready || busy())return false;
  Info held{},named{};
  if(metadata(nullptr,root_fd,held)<0 || inspect(root.c_str(),named)<0 ||
     !unchanged(root_info,held) || !unchanged(root_info,named))return fail(EAGAIN);
  for(auto *entry=entries.last();entry;entry=entry->previous){
    char path[1024];const int n=std::snprintf(path,sizeof(path),"%s/%s",root.c_str(),entry->path);
    Info current{};if(n<0 || size_t(n)>=sizeof(path) || !recovery_ancestors(root,entry->path) ||
       inspect(path,current)<0 || !unchanged(entry->info,current))return fail(EAGAIN);
  }
  if(metadata(nullptr,root_fd,held)<0 || !unchanged(root_info,held))return fail(EAGAIN);
  ready=false;state=State::copying;return true;
}
void Purge::cancel(){ready=false;state=State::cancelled;close();entries.clear();}
void Purge::step() {
  if(!busy())return;
  if(!remaining){state=State::complete;close();return;}
  Info named_root{};
  if(inspect(root.c_str(),named_root)<0 || !identity(root_info,named_root)){fail(EAGAIN);return;}
  const auto *entry=entries.last();if(!entry){fail(EIO);return;}
#ifdef _WIN32
  char path[1024];const int path_length=std::snprintf(path,sizeof(path),"%s/%s",root.c_str(),entry->path);
  if(path_length<0 || size_t(path_length)>=sizeof(path)){fail(ENAMETOOLONG);return;}
  if(!recovery_ancestors(root,entry->path)){fail(EAGAIN);return;}
  HANDLE handle=CreateFileA(path,DELETE|FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
  if(handle==INVALID_HANDLE_VALUE){fail(EACCES);return;}
  int fd=_open_osfhandle(reinterpret_cast<intptr_t>(handle),_O_BINARY|_O_RDONLY);
  if(fd<0){CloseHandle(handle);fail(EACCES);return;}
  Info actual{};
  const bool matches=metadata(nullptr,fd,actual)>=0 &&
      (entry->info.directory?identity(entry->info,actual):unchanged(entry->info,actual));
  FILE_DISPOSITION_INFO disposition{};disposition.DeleteFile=TRUE;
  const bool erased=matches && SetFileInformationByHandle(handle,FileDispositionInfo,&disposition,sizeof(disposition));
  close_file(nullptr,fd);
  if(!erased){fail(EACCES);return;}
#else
  const char *slash=std::strrchr(entry->path,'/');const char *name=slash?slash+1:entry->path;
  char parent[1024]{};if(slash){const size_t n=size_t(slash-entry->path);if(n>=sizeof(parent)){fail(ENAMETOOLONG);return;}std::memcpy(parent,entry->path,n);parent[n]=0;}
  int directory_fd=!slash ? ww::privilege::run([&]{return ::openat(root_fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);}) : anchored_directory(parent,root_fd);
  if(directory_fd<0){fail(EACCES);return;}
  int target=ww::privilege::run([&]{return ::openat(directory_fd,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|(entry->info.directory?O_DIRECTORY:0));});
  Info actual{};
  const bool matches=target>=0 && metadata(nullptr,target,actual)>=0 &&
      (entry->info.directory?identity(entry->info,actual):unchanged(entry->info,actual));
  bool erased=false;
  if(matches)erased=ww::privilege::run([&]{return ::unlinkat(directory_fd,name,entry->info.directory?AT_REMOVEDIR:0);})==0;
  if(target>=0)close_file(nullptr,target);
  if(erased)ww::privilege::run([&]{return ::fsync(directory_fd);});
  close_file(nullptr,directory_fd);
  if(!erased){fail(matches?EACCES:EAGAIN);return;}
#endif
  entries.pop_back();--remaining;++removed;
  if(!remaining){state=State::complete;close();}
}

bool ensure_folder(const char *path) {
  if(!path || !*path || std::strlen(path)>=1024)return false;
  int code=make_directory(path);
  Info info{};return (code==0 || code==-EEXIST) && inspect(path,info)>=0 && info.directory;
}
bool list_folders(const char *path,std::vector<std::string>&out,size_t limit){
  out.clear();std::vector<std::string> names;if(!children(path,names))return false;
  std::sort(names.begin(),names.end());
  for(const auto&name:names){if(name.empty()||name.front()=='.')continue;std::string child;if(!join_path(child,path,name))continue;Info info{};if(inspect(child.c_str(),info)>=0 && info.directory){out.push_back(name);if(out.size()>=limit)break;}}
  return true;
}
bool rename_item(const char *path,const Info &expected,const char *name,int &error){
  error=EINVAL;
  if(!expected.regular&&!expected.directory){error=ENOTSUP;return false;}
  if(!path||!name||!*name||std::strlen(name)>255||!std::strcmp(name,".")||!std::strcmp(name,"..")||[] (const char *value) { for(;*value;++value) if(std::strchr("/\\:*?\"<>|",*value)) return true; return false; }(name)||name[std::strlen(name)-1]==' '||name[std::strlen(name)-1]=='.')return false;
  std::string source(path);auto slash=source.find_last_of("/\\");if(slash==std::string::npos)return false;
  Info current{};if(inspect(path,current)<0||!unchanged(expected,current)){error=EAGAIN;return false;}
  if(source.substr(slash+1)==name){error=0;return true;}
  const std::string parent=source.substr(0,slash),target=parent+"/"+name;
  if(target.size()>=1024){error=ENAMETOOLONG;return false;}
  Info folder{};if(inspect(parent.c_str(),folder)<0||!folder.directory){error=EACCES;return false;}
#ifdef _WIN32
  if(!MoveFileExA(path,target.c_str(),MOVEFILE_WRITE_THROUGH)){error=GetLastError()==ERROR_ALREADY_EXISTS||GetLastError()==ERROR_FILE_EXISTS?EEXIST:EACCES;return false;}
#else
  // Reserve the destination exclusively; existing files/folders are refused.
  Info reserved{};int fd=-1,code=0;
  if(expected.directory)code=make_directory(target.c_str());
  else {fd=open_file(nullptr,target.c_str(),true);if(fd<0)code=fd;}
  if(code<0){error=-code;return false;}
  if(fd>=0){metadata(nullptr,fd,reserved);close_file(nullptr,fd);}else inspect(target.c_str(),reserved);
  auto release=[&]{Info actual{};if(inspect(target.c_str(),actual)>=0&&unchanged(reserved,actual))ww::privilege::run([&]{return std::remove(target.c_str());});};
  Info source_now{},parent_now{},target_now{};
  if(!reserved.inode||inspect(path,source_now)<0||!unchanged(expected,source_now)||inspect(parent.c_str(),parent_now)<0||!identity(folder,parent_now)||inspect(target.c_str(),target_now)<0||!unchanged(reserved,target_now)){release();error=EAGAIN;return false;}
  if(ww::privilege::run([&]{return std::rename(path,target.c_str());})!=0){error=directory::error_or_io(errno);release();return false;}
#endif
  Info after{};if(inspect(target.c_str(),after)<0||!identity(expected,after)){error=EAGAIN;return false;}
  error=0;return true;
}
} // namespace ww::fileops

namespace ww::fileops {
bool volume_mount_matches(const char *expected,const char *actual){
 if(!expected||!actual||!*expected||!*actual)return false;
 size_t a=std::strlen(expected),b=std::strlen(actual);while(a>1&&expected[a-1]=='/')--a;while(b>1&&actual[b-1]=='/')--b;
 return a==b&&!std::memcmp(expected,actual,a);
}
bool volume_space(const char *path,uint64_t &total,uint64_t &available,const char *required_mount){
 total=available=0;if(!path||!*path)return false;
#ifdef __PROSPERO__
 // Managed titles do not export ordinary statvfs/statfs/fstatfs. Match the
 // backend's checked libkernel query instead of calling an unsupported import.
 using Query=int(*)(int,struct statfs*);
 static const Query query=[] {void*symbol=nullptr;if(sceKernelDlsym(0x2001,"_fstatfs",&symbol)!=0)return Query{};return reinterpret_cast<Query>(symbol);}();
 if(!query)return false;
 struct statfs info{};int status=ww::privilege::run([&]{int fd=sceKernelOpen(path,O_RDONLY|O_DIRECTORY,0);if(fd<0)return -1;int result=query(fd,&info);sceKernelClose(fd);return result;});
 if(status!=0||!info.f_bsize||!info.f_blocks)return false;
 if(required_mount&&(!std::memchr(info.f_mntonname,0,sizeof(info.f_mntonname))||!volume_mount_matches(required_mount,info.f_mntonname)))return false;
 uint64_t block=info.f_bsize,blocks=info.f_blocks,free=info.f_bavail<0?0:uint64_t(info.f_bavail);
#elif defined(_WIN32)
 if(required_mount){char mounted[MAX_PATH]{};if(!GetVolumePathNameA(path,mounted,sizeof(mounted))||!volume_mount_matches(required_mount,mounted))return false;}
 ULARGE_INTEGER freeBytes{},totalBytes{},availableBytes{};if(!GetDiskFreeSpaceExA(path,&availableBytes,&totalBytes,&freeBytes))return false;total=totalBytes.QuadPart;available=availableBytes.QuadPart;return total&&available<=total;
#else
 if(required_mount){
#ifdef __linux__
  bool matched=false;FILE*mounts=std::fopen("/proc/self/mounts","r");if(!mounts)return false;
  while(auto item=getmntent(mounts))if(volume_mount_matches(required_mount,item->mnt_dir)){matched=true;break;}
  std::fclose(mounts);if(!matched)return false;
#else
  return false;
#endif
 }
 struct statvfs info{};if(ww::privilege::run([&]{return statvfs(path,&info);})!=0||!info.f_blocks)return false;
 uint64_t block=info.f_frsize?info.f_frsize:info.f_bsize,blocks=info.f_blocks,free=info.f_bavail;
#endif
#ifndef _WIN32
 if(!block||blocks>std::numeric_limits<uint64_t>::max()/block||free>blocks)return false;
 total=blocks*block;available=free*block;return true;
#endif
}
}

namespace ww::fileops {
namespace {
const char *profile_folders[]={"Desktop","Downloads","Documents","Pictures","Music","Videos"};
bool profile_path(const std::string&path){if(path.empty()||path[0]!='/'||path.size()>=900)return false;size_t at=1;while(at<path.size()){size_t end=path.find('/',at);if(end==std::string::npos)end=path.size();auto part=path.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}return path.back()!='/';}
bool profile_item_valid(const std::string&path){
 // Personal file IO already uses the app's qualified root gate. Keep newly
 // created entries owned by that creator: PS5 UID0 does not bypass UID1-only
 // permissions, so chowning this store to UID1 would break the gated IO path.
 Info info{};return inspect(path.c_str(),info)>=0&&(info.directory||info.regular);
}
bool profile_directory(const std::string&path,bool create){Info info{};int result=inspect(path.c_str(),info);if(result>=0)return info.directory;if(result!=-ENOENT||!create||make_directory(path.c_str())<0)return false;return profile_item_valid(path)&&inspect(path.c_str(),info)>=0&&info.directory;}
bool profile_chain(const std::string&path,bool create){for(size_t at=1;at<=path.size();++at)if(at==path.size()||path[at]=='/')if(!profile_directory(path.substr(0,at),create))return false;return true;}
bool profile_marker(const std::string&path,const std::string&value,bool write){const auto&io=system_io();int fd=io.open(io.context,path.c_str(),write);if(fd<0)return false;bool ok=true;if(write){size_t at=0;while(at<value.size()){auto n=io.write(io.context,fd,value.data()+at,value.size()-at);if(n<=0||uint64_t(n)>value.size()-at){ok=false;break;}at+=size_t(n);}if(ok)ok=io.sync(io.context,fd)==0;}else{char data[2048]{};auto n=io.read(io.context,fd,data,sizeof(data));ok=n==int64_t(value.size())&&!std::memcmp(data,value.data(),value.size());}if(io.close(io.context,fd)<0)ok=false;return ok;}
bool profile_sync(const std::string&path){const auto&io=system_io();int fd=io.open(io.context,path.c_str(),false);if(fd<0)return false;bool ok=io.sync(io.context,fd)==0;if(io.close(io.context,fd)<0)ok=false;return ok;}
}
bool personal_files_snapshot(const char*legacy,std::vector<TreeTransfer::Entry>&entries){entries.clear();if(!legacy||!*legacy)return false;for(const char*name:profile_folders){std::string path=std::string(legacy)+"/"+name;Info info{};int result=inspect(path.c_str(),info);if(result==-ENOENT)continue;if(result<0||!info.directory||!scan_tree(legacy,name,entries,0,true))return false;}return true;}
bool prepare_user_profile(const char*legacy_value,const char*console_value,const char*target_value,std::string&error){
 error="Invalid profile path";if(!legacy_value||!console_value||!target_value)return false;
 const std::string legacy=legacy_value,console=console_value,target=target_value;
 if(!profile_path(legacy)||!profile_path(console)||!profile_path(target)||target.compare(0,console.size()+1,console+"/")!=0||target.compare(0,legacy.size()+1,legacy+"/")==0)return false;
 if(!profile_chain(legacy,false)||!profile_chain(console,false)){error="Storage is unavailable";return false;}
 const std::string marker="WINBREW_USER_PROFILE_V1\n"+target+"\n";Info existing{};
 int target_status=inspect(target.c_str(),existing);
 if(target_status>=0){if(!existing.directory||!profile_chain(target,false)||!profile_marker(target+"/.winbrew-profile-v1",marker,false)){error="Existing profile is not a verified WinBrew profile";return false;}for(const char*name:profile_folders)if(!profile_directory(target+"/"+name,true)){error="Personal folder is unavailable";return false;}error.clear();return true;}
 if(target_status!=-ENOENT){error="Profile cannot be inspected";return false;}
 std::vector<TreeTransfer::Entry> before;error="Personal files contain an unsupported or changing item";if(!personal_files_snapshot(legacy.c_str(),before))return false;
 const std::string parent=target.substr(0,target.find_last_of('/')),stage=target+".migrating-v1";error="Profile staging area already exists or is unavailable";
 if(!profile_chain(parent,true)||make_directory(stage.c_str())<0||!profile_item_valid(stage))return false;
 for(const char*name:profile_folders){std::string source=legacy+"/"+name;Info info{};int status=inspect(source.c_str(),info);if(status==-ENOENT){if(!profile_directory(stage+"/"+name,true)){error="Cannot create personal folder";return false;}continue;}
  Clipboard clip;if(status<0||!clip.copy(source.c_str())){error="Personal folder changed";return false;}
  TreeTransfer transfer;if(!transfer.begin(clip,stage.c_str(),true)){error="Personal folder could not be copied";return false;}
  for(size_t i=0;transfer.busy()&&i<2000000;++i)transfer.step();
  if(transfer.state!=State::complete){if(transfer.busy())transfer.cancel();error=std::string("Personal copy failed: ")+transfer.stage;return false;}
  for(const auto&entry:transfer.outputs)if(!profile_item_valid(entry.relative)){error="Copied personal item is unavailable";return false;}
 }
 std::vector<TreeTransfer::Entry> after;error="Original personal files changed during migration";if(!personal_files_snapshot(legacy.c_str(),after)||!same_tree(before,after))return false;
 error="Profile verification marker failed";if(!profile_marker(stage+"/.winbrew-profile-v1",marker,true)||!profile_item_valid(stage+"/.winbrew-profile-v1")||!profile_marker(stage+"/.winbrew-profile-v1",marker,false)||!profile_sync(stage))return false;
 Info ready{};int code=0;error="Verified profile could not be published";if(inspect(stage.c_str(),ready)<0||!rename_item(stage.c_str(),ready,target.substr(target.find_last_of('/')+1).c_str(),code)||!profile_sync(parent))return false;
 error.clear();return true;
}
}
