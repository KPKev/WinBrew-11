// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>

namespace ww::notepad_search {
// An empty query never matches. Search wraps once at the end of the document.
inline size_t find_next(std::string_view text, std::string_view query, size_t start) {
  if (query.empty()) return std::string_view::npos;
  if (start > text.size()) start = text.size();
  size_t found = text.find(query, start);
  return found == std::string_view::npos ? text.find(query) : found;
}

// Build the entire result before committing, so a capacity failure loses no edits.
inline bool replace_all(std::string_view text, std::string_view query,
                        std::string_view replacement, size_t capacity,
                        std::string &result, size_t &count) {
  if (query.empty()) return false;
  std::string next;
  size_t pos = 0, matches = 0;
  while (true) {
    size_t found = text.find(query, pos);
    size_t end = found == std::string_view::npos ? text.size() : found;
    if (end - pos > capacity - next.size()) return false;
    next.append(text.substr(pos, end - pos));
    if (found == std::string_view::npos) break;
    if (replacement.size() > capacity - next.size()) return false;
    next.append(replacement);
    pos = found + query.size();
    ++matches;
  }
  result = std::move(next);
  count = matches;
  return true;
}
}
