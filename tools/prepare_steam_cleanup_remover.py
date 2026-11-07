"""Apply the experimental error-aware root check to a generated depot source.

This does not authorize cleanup of markerless folders or implement recovery.
"""
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit('usage: prepare_steam_cleanup_remover.py INPUT OUTPUT')
source = Path(sys.argv[1]).read_text()
include = '#include "steam/backend/depot_installer.h"'
if source.count(include) != 1:
    raise SystemExit('Pinned downloader include changed; refusing generation overlay')
source = source.replace(include, include + '\n#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)\n#include "steam_integration/install_generation.hpp"\n#endif')
old = '''  if (target.app_id == 0 || target.install_root.empty() || target.install_root[0] != '/' ||'''
new = '''#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
  const auto operation_generation = ww::steam_integration::install_generation::Current();
  if (!ww::steam_integration::install_generation::Valid(operation_generation))
    return fail(InstallError::kStorage, "missing install operation generation");
#endif
  if (target.app_id == 0 || target.install_root.empty() || target.install_root[0] != '/' ||'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader preflight changed; refusing generation overlay')
source = source.replace(old, new)
old = '''  std::fprintf(record, "{\\"version\\":1,\\"app_id\\":%u%s,\\"install_bytes\\":%llu,\\"files\\":%u,\\"depots\\":[",'''
new = '''#if defined(WINE11_INSTALL_GENERATION_EXPERIMENT)
  identity += ",\\"wine11_operation\\":\\"" + operation_generation + "\\"";
#endif
  std::fprintf(record, "{\\"version\\":1,\\"app_id\\":%u%s,\\"install_bytes\\":%llu,\\"files\\":%u,\\"depots\\":[",'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader record changed; refusing generation overlay')
source = source.replace(old, new)
old = '  if (!Exists(install_root)) return true;\n  // The copy of an imported game'
new = '''  struct stat root_info {};
  if (lstat(install_root.c_str(), &root_info) != 0) {
    if (errno == ENOENT) return true;
    return fail("The game's folder could not be inspected.");
  }
  if (!S_ISDIR(root_info.st_mode)) return fail("The game's folder is not a directory.");
  // The copy of an imported game'''
if source.count(old) == 1:
    source = source.replace(old, new)
elif source.count(new) != 1:
    raise SystemExit('Pinned remover changed; refusing ambiguous cleanup overlay')
old = 'namespace playden::steam {\nnamespace {'
new = '''namespace playden::steam {
// Experimental: called only after the installer wrote and renamed its record.
// It does not replace a durable winner journal or authorize cleanup.
bool SyncInstallData(const std::string& root, const std::string& relative, bool directory, std::uint64_t expected, bool check_size) {
  if (relative.empty() || relative.front() == '/' || relative.find('\\\\') != std::string::npos) return false;
  int root_fd = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (root_fd < 0) return false;
  std::vector<int> fds{root_fd};
  std::vector<std::string> names;
  struct stat root_stat{}, target_stat{}, named_stat{};
  bool ok = fstat(root_fd, &root_stat) == 0 && S_ISDIR(root_stat.st_mode);
  std::size_t pos = 0;
  while (ok && pos < relative.size()) {
    const auto slash = relative.find('/', pos);
    const bool last = slash == std::string::npos;
    const auto name = relative.substr(pos, last ? std::string::npos : slash - pos);
    if (name.empty() || name == "." || name == "..") { ok = false; break; }
    const int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | ((!last || directory) ? O_DIRECTORY : O_NONBLOCK);
    int fd = openat(fds.back(), name.c_str(), flags);
    if (fd < 0) { ok = false; break; }
    fds.push_back(fd);
    names.push_back(name);
    struct stat info{};
    if (fstat(fd, &info) != 0 || info.st_dev != root_stat.st_dev || ((!last || directory) ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode))) { ok = false; break; }
    if (last) {
      target_stat = info;
      if (check_size && (info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) != expected)) ok = false;
      if (ok && fsync(fd) != 0) ok = false;
      int named = ok ? openat(fds[fds.size() - 2], name.c_str(), flags) : -1;
      if (ok && (named < 0 || fstat(named, &named_stat) != 0 || target_stat.st_dev != named_stat.st_dev ||
                 target_stat.st_ino != named_stat.st_ino || target_stat.st_size != named_stat.st_size)) ok = false;
      if (named >= 0 && close(named) != 0) ok = false;
      break;
    }
    pos = slash + 1;
    if (pos == relative.size()) ok = false;
  }
  for (std::size_t i = fds.size(); ok && i > 1; --i) if (fsync(fds[i - 2]) != 0) ok = false;
  int root_check = ok ? open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) : -1;
  if (ok && (root_check < 0 || fstat(root_check, &named_stat) != 0 || root_stat.st_dev != named_stat.st_dev || root_stat.st_ino != named_stat.st_ino)) ok = false;
  int check_parent = root_check;
  for (std::size_t i = 0; ok && i < names.size(); ++i) {
    const bool last = i + 1 == names.size();
    const int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | ((!last || directory) ? O_DIRECTORY : O_NONBLOCK);
    int child = openat(check_parent, names[i].c_str(), flags);
    struct stat original{}, observed{};
    if (child < 0 || fstat(fds[i + 1], &original) != 0 || fstat(child, &observed) != 0 ||
        original.st_dev != observed.st_dev || original.st_ino != observed.st_ino || original.st_size != observed.st_size) ok = false;
    if (close(check_parent) != 0) ok = false;
    check_parent = child;
  }
  if (check_parent >= 0 && close(check_parent) != 0) ok = false;
  for (auto it = fds.rbegin(); it != fds.rend(); ++it) if (close(*it) != 0) ok = false;
  return ok;
}
bool SyncInstallWinner(const std::string& root) {
  struct stat named{}, opened{}, file{}, check_dir{}, check_file{};
  if (lstat(root.c_str(), &named) != 0 || !S_ISDIR(named.st_mode)) return false;
  int dir = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (dir < 0) return false;
  bool ok = fstat(dir, &opened) == 0 && named.st_dev == opened.st_dev && named.st_ino == opened.st_ino;
  int record = ok ? openat(dir, ".playden-install.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC) : -1;
  ok = ok && record >= 0 && fstat(record, &file) == 0 && S_ISREG(file.st_mode) && file.st_uid == geteuid() &&
      file.st_nlink == 1 && file.st_size > 0 && fsync(record) == 0 && fsync(dir) == 0;
  int again_dir = ok ? open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) : -1;
  int again_file = again_dir >= 0 ? openat(again_dir, ".playden-install.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC) : -1;
  ok = ok && again_dir >= 0 && again_file >= 0 && fstat(again_dir, &check_dir) == 0 && fstat(again_file, &check_file) == 0 &&
      opened.st_dev == check_dir.st_dev && opened.st_ino == check_dir.st_ino &&
      file.st_dev == check_file.st_dev && file.st_ino == check_file.st_ino && file.st_size == check_file.st_size;
  if (again_file >= 0 && close(again_file) != 0) ok = false;
  if (again_dir >= 0 && close(again_dir) != 0) ok = false;
  if (record >= 0 && close(record) != 0) ok = false;
  if (close(dir) != 0) ok = false;
  return ok;
}
namespace {'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader namespace changed; refusing winner sync overlay')
source = source.replace(old, new)
old = '''    if (std::fclose(list) != 0 || std::rename((list_path + ".tmp").c_str(), list_path.c_str()) != 0)'''
new = '''    const bool list_written = std::ferror(list) == 0;
    const bool list_closed = std::fclose(list) == 0;
    if (!list_written || !list_closed || std::rename((list_path + ".tmp").c_str(), list_path.c_str()) != 0)'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader file-list write changed; refusing data sync overlay')
source = source.replace(old, new)
old = '''  std::fclose(journal);
  journal = nullptr;
  stats.download_ms = Milliseconds(Clock::now() - download_start);'''
new = '''  // Flush every manifest-listed regular file and its descriptor-anchored parents.
  // Symlink entries are not installed by this downloader and are skipped.
  for (const auto& [key, placement] : placements) {
    const auto& file = *placement.file;
    if (file.is_symlink()) continue;
    if (!SyncInstallData(target.install_root, file.path, file.is_directory(), file.size, !file.is_directory()))
      return fail(InstallError::kStorage, "cannot durably sync install data");
  }
  if (!SyncInstallData(target.install_root, kFileList, false, 0, false))
    return fail(InstallError::kStorage, "cannot durably sync install file list");
  const bool journal_closed = std::fclose(journal) == 0;
  journal = nullptr;
  if (!journal_closed) return fail(InstallError::kStorage, "cannot close install journal");
  stats.download_ms = Milliseconds(Clock::now() - download_start);'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader completion changed; refusing data sync overlay')
source = source.replace(old, new)
old = '''  const bool closed = std::fclose(record) == 0;
  if (!closed || std::rename(temporary.c_str(), record_path.c_str()) != 0)
    return fail(InstallError::kStorage, "cannot write install record");
  std::remove(journal_path.c_str());'''
new = '''  const bool written = std::ferror(record) == 0;
  const bool closed = std::fclose(record) == 0;
  if (!written || !closed || std::rename(temporary.c_str(), record_path.c_str()) != 0 || !SyncInstallWinner(target.install_root))
    return fail(InstallError::kStorage, "cannot durably write install record");
  std::remove(journal_path.c_str());'''
if source.count(old) != 1:
    raise SystemExit('Pinned downloader winner finalization changed; refusing overlay')
source = source.replace(old, new)
Path(sys.argv[2]).write_text(source, encoding='utf-8', newline='\n')
