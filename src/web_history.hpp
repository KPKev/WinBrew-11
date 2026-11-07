// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
namespace ww {
// Privacy-limited launcher history. Only site origins are retained: no page
// paths, query strings, fragments, userinfo, cookies, or internal navigation.
struct WebHistory {
  static constexpr int Limit = 20;
  static constexpr size_t OriginBytes = 256;
  char sites[Limit][OriginBytes]{};
  int count = 0, error = 0;
  bool dirty = false;
  char path[1024]{};
  static bool origin(const char *url, char *out, size_t capacity);
  bool load(const char *storage_path);
  bool remember(const char *url);
  bool clear();
  bool save();
};
} // namespace ww
