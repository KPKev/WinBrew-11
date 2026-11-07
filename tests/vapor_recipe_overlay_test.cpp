// Test the actual pinned parser and profile renderer, including player values.
#include "runtime/game_settings.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(int argc, char** argv) {
  using namespace playden::runtime;
  CHECK(argc == 2);
  const std::filesystem::path root = argv[1];
  struct Case { unsigned app; const char* file; const char* executable; const char* graphics; };
  const Case cases[] = {
      {489830, "steam-489830-vapor-skyrimse-v1.json", "SkyrimSE.exe", "dxvk"},
      {105600, "steam-105600-vapor-terraria-v1.json", "Terraria.exe", "opengl"},
      {1942280, "steam-1942280-vapor-brotato-v1.json", "Brotato.exe", "opengl"},
      {365670, "steam-365670-vapor-blender-v1.json", "blender.exe", "opengl"},
      {240, "steam-240-vapor-cstrike-source-v1.json", "cstrike_win64.exe", "dxvk"}};
  KitCapabilities kit;
  kit.opengl_zink = kit.wine_mono = kit.environment = kit.profile_version_2 = true;
  for (const auto& test : cases) {
    std::ifstream file(root / test.file);
    CHECK(file.good());
    const std::string text{std::istreambuf_iterator<char>(file), {}};
    GameRecipe recipe;
    std::string reason;
    CHECK(ParseGameRecipe(text, &recipe, &reason));
    CHECK(recipe.app_id == test.app && recipe.locked.empty());
    EffectiveSettings values;
    SettingsError error;
    CHECK(BuildEffectiveSettings(recipe, nullptr, {}, kit, &values, &error));
    CHECK(values.launch && values.launch->executable == test.executable);
    CHECK(values.graphics == test.graphics);
    CHECK(values.values.at("steam_features").value == "emulate");
    if (test.app == 1942280) CHECK(values.arguments == "--rendering-driver opengl3");
    if (test.app == 365670) {
      CHECK(values.arguments == "--gpu-backend vulkan");
      CHECK(values.dll_overrides == "d3d8,d3d9,d3d10core,d3d11,dxgi=n");
    }
    ProsperoProfile profile;
    CHECK(RenderProsperoProfile(recipe, values, {"vapor-test", "/data/wine11/wine11-games/test"}, &profile, &reason));
    CHECK(profile.text.find(test.executable) != std::string::npos);

    // Saved player choices win; no writes to the settings store are involved.
    CHECK(BuildEffectiveSettings(recipe, nullptr,
        {{"program", "Custom.exe"}, {"extra_arguments", "-console"}, {"graphics", "gdi"}, {"dll_overrides", ""},
         {"resolution", "1280x720"}, {"steam_features", "game"}}, kit, &values, &error));
    CHECK(values.launch && values.launch->executable == "Custom.exe");
    CHECK(values.graphics == "gdi" && values.values.at("graphics").source == ValueSource::kUser);
    CHECK(values.values.at("steam_features").value == "game");
    CHECK(values.values.at("resolution").value == "1280x720");
    CHECK(values.dll_overrides.empty());
    CHECK(values.arguments.find("-console") != std::string::npos);
    if (test.app == 1942280) CHECK(values.arguments.find("opengl3") == std::string::npos);
    CHECK(RenderProsperoProfile(recipe, values, {"vapor-test", "/data/wine11/wine11-games/test"}, &profile, &reason));
    CHECK(profile.text.find("Custom.exe") != std::string::npos);
  }
  std::puts("Vapor recipes parse, render and preserve explicit player overrides.");
}
