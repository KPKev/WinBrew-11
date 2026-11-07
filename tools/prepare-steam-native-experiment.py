"""Generate an isolated compile-only native Wine 11 + Steam integration probe.

This does not change the shared staging tree or produce a deployable title.
The app-mount layout and full title still need validation on the console.
"""
from pathlib import Path
import os

root = Path(__file__).resolve().parents[1]
source = (root / 'src/ps5.cpp').read_text()
game_host = os.environ.get('WINE11_EXPERIMENT_GAME_HOST') == '1'


def replace_once(old, new):
    global source
    if source.count(old) != 1:
        raise SystemExit('native source changed; refusing ambiguous experiment transformation')
    source = source.replace(old, new, 1)


includes = '#include "shell.hpp"\n#include "steam_integration/native_bridge.hpp"\n#include "steam_integration/native_app_mount.hpp"'
if game_host:
    includes += '\n#include "steam_integration/native_game_startup.hpp"\n#include "steam_integration/native_title_provision.hpp"\n#include "platform/ps5/console_services.h"\n#include "platform/ps5/title_runtime.h"\n#include <cerrno>'
    includes += '\nextern "C" int sceSystemServiceLoadExec(const char*, const char**);'
    includes += '\n#include "steam_integration/app_user_identity.hpp"\n#include "steam_integration/vapor_privilege.hpp"\n#include "privilege.hpp"'
replace_once('#include "shell.hpp"', includes)
replace_once('static ww::Shell shell;', '''static ww::Shell shell;
static char steam_root[1024]{};''' + ('\nstatic std::unique_ptr<ww::steam_integration::NativeGameStartup> steam_game;' if game_host else '\nstatic std::unique_ptr<ww::steam_integration::NativeBridge> steam_bridge;'))
replace_once('      shell.load_preferences();', '''      std::snprintf(steam_root, sizeof(steam_root), "%s", path);
      shell.load_preferences();''')
game_tick = '''  if (steam_game) {
    static int last_game_state = -1;
    const auto game_state = steam_game->Tick();
    const int observed = static_cast<int>(game_state);
    if (observed != last_game_state) {
      log("Steam game startup state", static_cast<uint32_t>(observed));
      last_game_state = observed;
    }
  }'''
replace_once('  shell.tick();', (game_tick if game_host else '  if (steam_bridge) steam_bridge->Tick();') + '\n  shell.tick();')
if game_host:
    replace_once('  shell.tick();', '''  shell.tick();
  if (shell.steam_finish_setup_requested) {
    shell.steam_finish_setup_requested = false;
    // Stop Steam's workers before asking the console to end the title.
    // Returning from main or calling exit() is not supported in this context.
    steam_game.reset();
    native_input::close();
    if (audio >= 0) { sceAudioOutClose(audio); audio = -1; shell.audio_available = false; }
    const int status = sceSystemServiceLoadExec("exit", nullptr);
    log("finish setup exit", static_cast<uint32_t>(status));
    shell.result(false, static_cast<uint32_t>(status), "Could not return to Home; close WinBrew 11 from the PS5 menu");
  }''')
replace_once('  int private_dir = open("/download0", O_RDONLY | O_DIRECTORY);', '''  int private_dir = open("/download0", O_RDONLY | O_DIRECTORY);
  int app_dir = open("/app0", O_RDONLY | O_DIRECTORY);''' + ('\n  int data_dir = -1;\n  int data_anchor_error = 0;' if game_host else ''))
if game_host:
    replace_once('  auto elevation_status = elevation::request(elevation::Capability::filesystem);',
                 '''  // Playden's credential model: remember the app user before the grant.
  const uid_t original_euid = geteuid();
  auto elevation_status = elevation::request(elevation::Capability::filesystem);''')
replace_once('  bool private_bound = bind_private_paths(private_dir);', '''  bool private_bound = bind_private_paths(private_dir);
''' + ('''  // Before elevation /data belongs to the app sandbox, not the console data
  // mount. Anchor the global path only after the root grant has succeeded.
  if (elevated) {
    data_dir = open("/data", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    data_anchor_error = data_dir < 0 ? errno : 0;
  } else {
    data_anchor_error = EACCES;
  }
  // Run as the app user from here on, like Playden: hand earlier root-created
  // Vapor files over once, lower the effective UID, and raise root only for
  // specific steps. Must happen before any Steam/Playden thread starts.
  {
    ww::steam_integration::AppUserPaths user_paths{steam_root};
    const auto identity = ww::steam_integration::AdoptAndLower(original_euid, elevated && private_bound, user_paths);
    log(ww::steam_integration::AppUserStageName(identity.stage), static_cast<uint32_t>(identity.error));
    log("app user uid", static_cast<uint32_t>(identity.app_uid));
    if (identity.adopted_now) {
      log("app user adopted entries", static_cast<uint32_t>(identity.stats.changed));
      log("app user refused entries", static_cast<uint32_t>(identity.stats.refused));
      log("app user errors", static_cast<uint32_t>(identity.stats.errors));
    }
    if (identity.stage == ww::steam_integration::AppUserStage::lowered)
      ww::privilege::install(ww::steam_integration::VaporRaise);
  }
''' if game_host else '') + '''
  char steam_ca_path[1024]{};
  const char* app_roots[] = {"/app0", "/mnt/sandbox/PPSA77381_000/app0"};
  bool app_bound = ww::steam_integration::ResolvePackagedCa(app_dir, app_roots, 2, steam_ca_path, sizeof(steam_ca_path));
  if (app_dir >= 0) close(app_dir);''')
replace_once('  if (private_dir >= 0)\n    close(private_dir);', '  // Keep the anchor open until the experimental Steam worker duplicates it.')
if game_host:
    startup = '''  open_audio();
  // Isolated research build only. A missing data anchor leaves account UI
  // available but disables the separate game host.
  log("Steam data anchor error", static_cast<uint32_t>(data_anchor_error));
  if (private_bound && app_bound && private_dir >= 0) {
    std::string ca(steam_ca_path), suffix("/assets/steam/cacert.pem");
    if (ca.size() > suffix.size() && ca.compare(ca.size() - suffix.size(), suffix.size(), suffix) == 0) {
      auto runtime = ca.substr(0, ca.size() - suffix.size()) + "/runtime";
      steam_game = std::make_unique<ww::steam_integration::NativeGameStartup>(shell);
      auto launch = [](const std::string& title, const std::vector<std::string>& args) {
        static auto title_api = [] {
          auto resolved = playden::ps5::ResolveTitleApi(
            {sceSysmoduleLoadModuleInternal, sceSysmoduleGetModuleHandleInternal, sceKernelDlsym},
            sceUserServiceGetForegroundUser);
          if (!resolved.launch && !resolved.public_launch) {
            resolved.public_launch = sceSystemServiceLaunchApp;
            resolved.foreground_user = sceUserServiceGetForegroundUser;
          }
          return resolved;
        }();
        static playden::ps5::TitleBackend backend(title_api);
        return backend.Launch(title, args);
      };
      const bool started = steam_game->Start(steam_root, private_dir, steam_ca_path, std::move(runtime),
        ww::steam_integration::MakeConsoleTitleProvision(data_dir), playden::ps5::ShadowMountPost, launch,
        data_dir >= 0 ? data_dir : -2);
      log("Steam startup accepted", started ? 0 : 1);
    }
  }
  if (data_dir >= 0) close(data_dir);
  if (private_dir >= 0) close(private_dir);
  ps5::demo::run_frames'''
else:
    startup = '''  open_audio();
  // Compile probe only: the package mount must be immutable on the console.
  if (private_bound && app_bound && private_dir >= 0) {
    steam_bridge = std::make_unique<ww::steam_integration::NativeBridge>(shell);
    steam_bridge->Start(steam_root, private_dir, steam_ca_path);
  }
  if (private_dir >= 0) close(private_dir);
  ps5::demo::run_frames'''
replace_once('  open_audio();\n  ps5::demo::run_frames', startup)
out = root / 'build/steam-native-experiment'
out.mkdir(parents=True, exist_ok=True)
(out / 'ps5.cpp').write_text(source)
print('Generated isolated native compile probe; do not deploy.')
