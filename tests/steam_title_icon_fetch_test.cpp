// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/title_icon_fetch.hpp"
#include <cassert>
#include <vector>

int main() {
  using ww::steam_integration::FetchTitleIcon;
  std::vector<std::string> requests;
  auto load = [&](const std::string& url) -> std::optional<std::string> {
    requests.push_back(url);
    if (url.ends_with("library_hero.jpg")) return "game-specific-png";
    return std::nullopt;
  };
  assert(!FetchTitleIcon(0, load) && requests.empty());
  assert(FetchTitleIcon(730, load) == "game-specific-png");
  assert(requests.size() == 2);
  assert(requests[0] == "https://cdn.cloudflare.steamstatic.com/steam/apps/730/library_600x900.jpg");
  assert(requests[1] == "https://cdn.cloudflare.steamstatic.com/steam/apps/730/library_hero.jpg");
  requests.clear();
  assert(!FetchTitleIcon(489830, [&](const std::string& url) -> std::optional<std::string> {
    requests.push_back(url); return std::nullopt;
  }));
  assert(requests.size() == 2 && requests[1].find("/489830/") != std::string::npos);
}
