// SPDX-License-Identifier: GPL-3.0-or-later
// Check the real pinned recipe parser against the packaged catalog, not mocks.
#include "runtime/game_settings.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  assert(argc == 2);
  std::vector<playden::runtime::SettingsPreset> presets;
  std::vector<playden::runtime::GameRecipe> recipes;
  std::string reason;
  assert(playden::runtime::LoadSettingsData(argv[1], &presets, &recipes, &reason));
  assert(!presets.empty() && recipes.size() == 4);
  std::map<unsigned, std::string> expected = {
    {220, "steam-220-hl2-v1"}, {292120, "steam-292120-ffxiii-v1"},
    {2640950, "steam-2640950-scottpilgrimex-v1"}, {4241920, "steam-4241920-braziliandrugdealer4-v1"}};
  for (const auto &recipe : recipes) {
    assert(expected.count(recipe.app_id) && expected.at(recipe.app_id) == recipe.id);
    expected.erase(recipe.app_id);
    if (recipe.app_id == 292120) {
      assert(recipe.graphics == "dxvk" && recipe.defaults.at("steam_features") == "emulate");
      bool registry_found = false;
      for (const auto &value : recipe.registry)
        if (value.key == "Software\\Wine\\AppDefaults\\ffxiiiimg.exe\\Direct3D" &&
            value.name == "VideoMemorySize" && value.data == "\"4096\"") registry_found = true;
      assert(registry_found);
    }
  }
  assert(expected.empty());
  std::puts("PASS: all pinned shipped compatibility recipes and FFXIII registry fix parse from staged runtime");
}
