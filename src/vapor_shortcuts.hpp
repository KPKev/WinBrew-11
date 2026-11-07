// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>

namespace ww {
// A shortcut records a Steam application ID, never credentials or game files.
class VaporShortcuts {
 public:
  enum Placement : unsigned { Desktop = 1, Start = 2, Taskbar = 4, Favorite = 8 };
  struct Entry { std::uint32_t app = 0; unsigned placements = 0; };
  static constexpr std::size_t Capacity = 128;
  bool load(const char* preferences_path);
  bool toggle(std::uint32_t app, Placement placement);
  unsigned placements(std::uint32_t app) const;
  const Entry* entries() const { return entries_; }
  std::size_t count() const { return count_; }
 private:
  bool save(const Entry* entries, std::size_t count) const;
  char path_[1100]{};
  Entry entries_[Capacity]{};
  std::size_t count_ = 0;
  bool valid_ = false;
};
}
