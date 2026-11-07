// SPDX-License-Identifier: GPL-3.0-or-later
// Vapor's per-game controller layouts must be values Playden itself accepts,
// and must render the intended [input] section of the game's profile.
#include "../src/steam_integration/controller_layouts.hpp"
#include "runtime/game_settings.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace ww::steam_integration;
using namespace playden::runtime;

static std::string read_text(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
static std::string input_section(const std::string &profile) {
  const auto start = profile.find("[input]");
  assert(start != std::string::npos);
  const auto end = profile.find("\n[", start + 1);
  return profile.substr(start, end == std::string::npos ? std::string::npos : end - start);
}
static bool has_line(const std::string &text, const std::string &line) {
  return text.find("\n" + line + "\n") != std::string::npos || text.rfind(line + "\n", 0) == 0;
}

int main(int argc, char **argv) {
  const std::string playden = argc > 1 ? argv[1] : "vendor/playden-source";
  std::string reason;
  GameRecipe recipe;
  assert(ParseGameRecipe(read_text(playden + "/runtime/recipes/steam-220-hl2-v1.json"), &recipe, &reason));
  KitCapabilities kit;
  kit.input_tuning = true;  // current kits; pointer rows need it
  const ProfileTarget target{"wine11-steam-220", "/data/wine11/wine11-games/220"};

  GameRecipe skyrim;
  assert(ParseGameRecipe(read_text(playden + "/runtime/recipes/steam-220-hl2-v1.json"), &skyrim, &reason));
  skyrim.id = "generated-steam-489830";
  skyrim.app_id = 489830;
  skyrim.name = "The Elder Scrolls V: Skyrim Special Edition";
  skyrim.launch_entries = {{"launch-1", "Skyrim Special Edition", "SkyrimSELauncher.exe", "", ""}};
  skyrim.architecture = "pe64";
  const ProfileTarget skyrim_target{"wine11-steam-489830", "/data/wine11/wine11-games/489830"};

  for (int i = 0; i < kControllerLayoutCount; ++i) {
    const auto layout = static_cast<ControllerLayout>(i);
    const auto overrides = ControllerLayoutOverrides(layout);
    for (const auto &[id, value] : overrides) {
      if (!ValidateSettingValue(recipe, id, value, &reason)) {
        std::fprintf(stderr, "layout %s: %s=%s refused: %s\n", ControllerLayoutName(layout), id.c_str(), value.c_str(), reason.c_str());
        return 1;
      }
    }
    EffectiveSettings settings;
    SettingsError error;
    assert(BuildEffectiveSettings(recipe, nullptr, overrides, kit, &settings, &error));
    ProsperoProfile profile;
    assert(RenderProsperoProfile(recipe, settings, target, &profile, &reason));
    const auto input = input_section(profile.text);
    std::printf("--- %s\n%s\n", ControllerLayoutName(layout), input.c_str());
    switch (layout) {
      case ControllerLayout::gamepad:
        assert(has_line(input, "mode = xinput") && has_line(input, "mouse = none"));
        break;
      case ControllerLayout::gamepad_pointer:
        assert(has_line(input, "mode = xinput") && has_line(input, "mouse = right_stick"));
        assert(has_line(input, "touchpad = mouse_left"));
        break;
      case ControllerLayout::mouse:
        assert(has_line(input, "preset = mouse"));
        break;
      case ControllerLayout::arrows:
        assert(has_line(input, "preset = arrows"));
        break;
    }
    assert(NextControllerLayout(layout) == static_cast<ControllerLayout>((i + 1) % kControllerLayoutCount));
    assert(*ControllerLayoutName(layout) && *ControllerLayoutDescription(layout));
  }

  // Durable store.
  char tmpl[] = "/tmp/vapor-controls-XXXXXX";
  assert(mkdtemp(tmpl));
  const std::string folder = tmpl;
  {
    ControllerLayoutStore store(folder);
    assert(store.Load());                                  // missing file: empty, fine
    assert(store.Get(489830) == ControllerLayout::gamepad);
    // Skyrim starts its game executable directly. Steam's launcher reports an
    // invalid handle from ShellExecuteEx on this runtime.
    const auto skyrim_default = store.Record(489830);
    assert(skyrim_default && skyrim_default->revision > 3);
    assert(skyrim_default->overrides.at("controller") == "xinput");
    assert(skyrim_default->overrides.at("controller_preset") == "none");
    assert(skyrim_default->overrides.at("program") == "SkyrimSE.exe");
    assert(skyrim_default->overrides.at("launch_option") == kProgramEntryId);
    assert(!store.Record(3594780));                        // other games keep Playden defaults
    assert(store.Set(489830, ControllerLayout::gamepad_pointer) == 1);
    assert(store.Set(489830, ControllerLayout::mouse) == 2);  // revisions only grow
    assert(store.Set(3594780, ControllerLayout::arrows) == 1);
    assert(store.Set(0, ControllerLayout::mouse) == 0);
    const auto record = store.Record(489830);
    auto skyrim_mouse = ControllerLayoutOverrides(ControllerLayout::mouse);
    skyrim_mouse["program"] = "SkyrimSE.exe";
    skyrim_mouse["launch_option"] = kProgramEntryId;
    assert(record && record->revision == skyrim_default->revision + 2 && record->overrides == skyrim_mouse);
    // A previous build published the same saved layout at revision 2: it must
    // receive the executable migration without another controller-button click.
    assert(record->revision > 2);
  }
  {
    ControllerLayoutStore reread(folder);
    assert(reread.Load());
    assert(reread.Get(489830) == ControllerLayout::mouse && reread.Get(3594780) == ControllerLayout::arrows);
    assert(reread.Set(489830, ControllerLayout::gamepad) == 3);  // back to default still publishes a newer revision
    auto skyrim_gamepad = ControllerLayoutOverrides(ControllerLayout::gamepad);
    skyrim_gamepad["program"] = "SkyrimSE.exe";
    skyrim_gamepad["launch_option"] = kProgramEntryId;
    assert(reread.Record(489830)->overrides == skyrim_gamepad);
    assert(reread.Record(489830)->revision == (1ull << 32) + 3);
    EffectiveSettings settings;
    SettingsError error;
    assert(BuildEffectiveSettings(skyrim, nullptr, reread.Record(489830)->overrides, kit, &settings, &error));
    assert(settings.launch && settings.launch->id == kProgramEntryId && settings.launch->executable == "SkyrimSE.exe");
    ProsperoProfile profile;
    assert(RenderProsperoProfile(skyrim, settings, skyrim_target, &profile, &reason));
    assert(has_line(input_section(profile.text), "mode = xinput"));
    struct stat st{};
    assert(stat((folder + "/controller-layouts.v1").c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
  }
  {
    std::ofstream(folder + "/controller-layouts.v1") << "wine11-controller-layouts 1\n489830 jetpack 1\n";
    ControllerLayoutStore bad(folder);
    assert(!bad.Load() && bad.Get(489830) == ControllerLayout::gamepad);  // malformed: defaults, nothing applied
    std::ofstream(folder + "/controller-layouts.v1") << "something else\n";
    assert(!bad.Load());
  }
  assert(std::system(("rm -rf '" + folder + "'").c_str()) == 0);
  std::puts("PASS: Vapor controller layouts are accepted by Playden, render the intended [input] profile, and persist durably");
}
