// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/game_settings.h"
#include "../src/steam_integration/native_app_mount.hpp"
#include "../src/steam_integration/native_trust.hpp"
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
int main(int argc, char** argv) {
  std::vector<playden::runtime::SettingsPreset> presets;
  std::vector<playden::runtime::GameRecipe> recipes;
  std::string reason;
  const auto app = std::filesystem::absolute(argc > 1 ? argv[1] : "build/steam-app-assets-candidate").string();
  int anchor = open(app.c_str(), O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  const char* roots[] = {app.c_str()}; char ca[1024];
  assert(ww::steam_integration::ResolvePackagedCa(anchor, roots, 1, ca, sizeof(ca)));
  assert(ww::steam_integration::VerifyPackagedSteamCa(ca));
  assert(close(anchor) == 0);
  assert(playden::runtime::LoadSettingsData(app + "/runtime", &presets, &recipes, &reason));
  assert(!presets.empty() && !recipes.empty());
}
