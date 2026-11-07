// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace ww::steam_integration {
// The supplied loader handles fetching, validation and PNG encoding. Each
// candidate remains bound to the selected Steam app, including the fallback.
template <typename Loader>
std::optional<std::string> FetchTitleIcon(std::uint32_t app, Loader&& load) {
  if (!app) return std::nullopt;
  const auto base = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + std::to_string(app) + "/";
  for (const char* name : {"library_600x900.jpg", "library_hero.jpg"})
    if (auto result = load(base + name)) return result;
  return std::nullopt;
}
}
