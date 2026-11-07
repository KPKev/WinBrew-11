// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_list_merge.hpp"
#include <set>
namespace ww::steam_integration {
namespace {
constexpr std::size_t kMaxList = 65536;
bool Add(std::string_view text, std::set<std::string>& titles) {
  if (text.size() >= kMaxList) return false;
  for (std::size_t pos = 0; pos < text.size();) {
    const auto end = text.find('\n', pos);
    if (end == std::string_view::npos || end - pos != 9) return false;
    auto id = text.substr(pos, 9);
    if (id.substr(0, 6) != "FAKE78") return false;
    for (std::size_t n = 6; n < 9; ++n) if (id[n] < '0' || id[n] > '9') return false;
    titles.emplace(id); pos = end + 1;
  }
  return true;
}
}
std::optional<std::string> MergeTitleLists(std::string_view existing, std::string_view requested) {
  std::set<std::string> titles;
  if (!Add(existing, titles) || !Add(requested, titles)) return std::nullopt;
  std::string result;
  for (const auto& id : titles) { result += id; result += '\n'; }
  if (result.size() >= kMaxList) return std::nullopt;
  return result;
}
}
