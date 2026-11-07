// SPDX-License-Identifier: GPL-3.0-or-later
#include "vapor_shortcuts.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ww {
bool VaporShortcuts::load(const char* preferences_path) {
  count_ = 0; valid_ = false; path_[0] = 0;
  if (!preferences_path || !*preferences_path) return false;
  const char* slash = std::strrchr(preferences_path, '/');
#ifdef _WIN32
  const char* backslash = std::strrchr(preferences_path, '\\');
  if (backslash && (!slash || backslash > slash)) slash = backslash;
#endif
  if (!slash) return false;
  const auto prefix = std::size_t(slash - preferences_path + 1);
  constexpr char name[] = "vapor-shortcuts.txt";
  if (prefix + sizeof(name) > sizeof(path_)) return false;
  std::memcpy(path_, preferences_path, prefix); std::memcpy(path_ + prefix, name, sizeof(name));
  errno = 0; FILE* file = std::fopen(path_, "rb");
  if (!file) { valid_ = errno == ENOENT; return valid_; }
  char text[4096]{}; const auto n = std::fread(text, 1, sizeof(text) - 1, file);
  const bool ok = !std::ferror(file) && std::feof(file); std::fclose(file);
  if (!ok || n < 3 || std::memchr(text, 0, n) || std::memcmp(text, "V1\n", 3)) return false;
  const char* p = text + 3; Entry loaded[Capacity]{}; std::size_t count = 0;
  while (*p) {
    unsigned long long id = 0; unsigned flags = 0; int consumed = 0;
    if (count == Capacity || std::sscanf(p, "%llu %u%n", &id, &flags, &consumed) != 2 ||
        consumed <= 0 || p[consumed] != '\n' || !id || id > UINT32_MAX || !flags || flags > 15) return false;
    for (std::size_t i = 0; i < count; ++i) if (loaded[i].app == id) return false;
    loaded[count++] = {static_cast<std::uint32_t>(id), flags}; p += consumed + 1;
  }
  std::memcpy(entries_, loaded, sizeof(loaded)); count_ = count; valid_ = true; return true;
}
unsigned VaporShortcuts::placements(std::uint32_t app) const {
  for (std::size_t i = 0; i < count_; ++i) if (entries_[i].app == app) return entries_[i].placements;
  return 0;
}
bool VaporShortcuts::save(const Entry* entries, std::size_t count) const {
  char data[4096] = "V1\n"; std::size_t used = 3;
  for (std::size_t i = 0; i < count; ++i) {
    int n = std::snprintf(data + used, sizeof(data) - used, "%u %u\n", entries[i].app, entries[i].placements);
    if (n < 0 || std::size_t(n) >= sizeof(data) - used) return false;
    used += std::size_t(n);
  }
  char temp[1150]; if (std::snprintf(temp, sizeof(temp), "%s.tmp", path_) >= int(sizeof(temp))) return false;
  FILE* file = std::fopen(temp, "wb"); if (!file) return false;
  bool ok = std::fwrite(data, 1, used, file) == used && std::fflush(file) == 0;
#ifdef _WIN32
  if (ok) ok = _commit(_fileno(file)) == 0;
#else
  if (ok) ok = ::fsync(fileno(file)) == 0;
#endif
  if (std::fclose(file) != 0) ok = false;
  if (!ok) return false;
#ifdef _WIN32
  return MoveFileExA(temp, path_, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return std::rename(temp, path_) == 0;
#endif
}
bool VaporShortcuts::toggle(std::uint32_t app, Placement placement) {
  const unsigned bit = static_cast<unsigned>(placement);
  if (!valid_ || !app || (bit != Desktop && bit != Start && bit != Taskbar && bit != Favorite)) return false;
  if (bit != Favorite && !(placements(app) & bit)) {
    std::size_t used = 0;
    for (std::size_t j = 0; j < count_; ++j) if (entries_[j].placements & bit) ++used;
    if (used >= (bit == Taskbar ? 3u : 4u)) return false;
  }
  Entry next[Capacity]; std::memcpy(next, entries_, sizeof(next)); auto count = count_;
  std::size_t i = 0; while (i < count && next[i].app != app) ++i;
  if (i == count) { if (count == Capacity) return false; next[count++] = {app, 0}; }
  next[i].placements ^= bit;
  if (!next[i].placements) { for (std::size_t j = i + 1; j < count; ++j) next[j - 1] = next[j]; --count; }
  if (!save(next, count)) return false;
  std::memcpy(entries_, next, sizeof(next)); count_ = count; return true;
}
}
