// SPDX-License-Identifier: GPL-3.0-or-later
#include "elevation.hpp"
#include "ps5_browser.hpp"
#include "ps5_hid.hpp"
#include "ps5_video.hpp"
#include "shell.hpp"
#include "owner_check_dispatch.hpp"
#include "title_launch.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
// scePad sample ABI from ps5-homebrew-ui/platform/ps5/pad.cpp
// (GPL-3.0-or-later).
struct RawPadTouch { uint16_t x, y; uint8_t finger, reserved[3]; };
struct RawPadTouchData { uint8_t fingers, reserved0[3]; uint32_t reserved1; RawPadTouch touch[2]; };
struct RawPad {
  uint32_t buttons;
  uint8_t lx, ly, rx, ry, l2, r2, reserved[42];
  RawPadTouchData touch_data;
  int32_t connected;
  uint64_t timestamp;
  uint8_t ext[16], count, pad[15];
};
static_assert(sizeof(RawPad) == 120 && offsetof(RawPad, touch_data) == 0x34 &&
              offsetof(RawPad, connected) == 0x4c);
extern "C" {
int sceUserServiceInitialize(const void *);
int sceUserServiceGetInitialUser(int *);
int sceUserServiceGetForegroundUser(uint32_t *);
int scePadInit();
int scePadOpen(int, int, int, const void *);
int scePadRead(int, RawPad *, int);
int sceSysmoduleLoadModuleInternal(uint32_t);
int sceSysmoduleGetModuleHandleInternal(uint32_t, int32_t *);
int sceKernelDlsym(int32_t, const char *, void **);
uint32_t sceSystemServiceLaunchApp(const char *, const char **, void *);
int sceSystemServiceLaunchWebBrowser(const char *, void *);
int sceNetCtlInit();
int sceNetCtlGetState(int *);
int sceNetCtlGetInfo(int, void *);
int sceAudioOutInit();
int sceAudioOutOpen(int, int, int, unsigned, unsigned, unsigned);
int sceAudioOutOutput(int, const void *);
int sceAudioOutSetVolume(int, int, const int32_t *);
int sceAudioOutClose(int);
int sceKernelUsleep(uint32_t);
}
static ww::Shell shell;
// Fixed desktop working set: no large allocation from the title startup heap.
alignas(64) static uint32_t desktop_frame[ww::W * ww::H];
alignas(64) static uint32_t desktop_background[ww::W * ww::H];
alignas(64) static unsigned char desktop_font[ww::AtlasBytes];
static int pad = -1;
static bool elevated = false;
static ww::TitleApi api;
static char log_path[1024] = "/download0/wine11.log";
static int audio = -1;
static int (*big_app)() = nullptr;
static int (*app_title)(uint32_t, char *) = nullptr;
static double last_frame = 0, last_status = 0;
static double monotonic() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}
static void log(const char *stage, uint32_t code) {
  FILE *f = std::fopen(log_path, "a");
  if (f) {
    std::fprintf(f, "%s code=0x%08X\n", stage, code);
    std::fclose(f);
  }
}
// Lapy can change the process root. Bind the same private directory through
// its actual global path, checking the anchored directory's device and inode.
static bool bind_private_paths(int anchored) {
  struct stat original{}, candidate{};
  if (anchored < 0 || fstat(anchored, &original) != 0)
    return false;
  const char *paths[] = {"/download0", "/mnt/sandbox/PPSA77381_000/download0"};
  for (auto path : paths)
    if (stat(path, &candidate) == 0 && candidate.st_dev == original.st_dev &&
        candidate.st_ino == original.st_ino) {
      std::snprintf(log_path, sizeof(log_path), "%s/wine11.log", path);
      std::snprintf(shell.preferences, sizeof(shell.preferences),
                    "%s/desktop-settings.ini", path);
      shell.load_preferences();
      shell.vapor_shortcuts.load(shell.preferences);
      return true;
    }
  return false;
}
static void open_audio() {
  auto code = sceAudioOutInit();
  if (code != 0 && uint32_t(code) != 0x8026000eu) {
    log("audio init", uint32_t(code));
    return;
  }
  audio = sceAudioOutOpen(0xff, 0, 0, 256, 48000, 1);
  shell.audio_available = audio >= 0;
  log("desktop audio", uint32_t(audio));
}
static void test_sound() {
  int32_t volumes[8]{};
  for (auto &v : volumes)
    v = shell.ui_muted ? 0 : 0x8000 * shell.ui_volume / 100;
  if (sceAudioOutSetVolume(audio, 3, volumes) < 0) {
    shell.audio_available = false;
    return;
  }
  alignas(64) int16_t samples[256 * 2]{};
  for (int part = 0; part < 4; ++part) {
    for (int i = 0; i < 256; ++i) {
      int at = part * 256 + i;
      float envelope = std::sin(float(at) * 3.14159265f / 1024);
      auto sample =
          int16_t(std::sin(float(at) * 2 * 3.14159265f * 660 / 48000) *
                  envelope * 3800);
      samples[i * 2] = samples[i * 2 + 1] = sample;
    }
    if (sceAudioOutOutput(audio, samples) < 0) {
      shell.audio_available = false;
      break;
    }
  }
}
static void refresh_status() {
  int state = -1;
  shell.network_state = sceNetCtlGetState(&state) == 0 ? state : -1;
  alignas(8) unsigned char info[256]{};
  if (shell.network_state == 3) {
    if (sceNetCtlGetInfo(14, info) == 0) {
      info[15] = 0;
      std::snprintf(shell.network_ip, sizeof(shell.network_ip), "%s",
                    reinterpret_cast<char *>(info));
    }
    std::memset(info, 0, sizeof(info));
    if (sceNetCtlGetInfo(1, info) == 0) {
      uint32_t device = 0;
      std::memcpy(&device, info, 4);
      std::snprintf(shell.network_name, sizeof(shell.network_name), "%s",
                    device == 1 ? "Wi-Fi" : "Ethernet");
    }
  } else
    shell.network_ip[0] = 0;
  if (big_app && app_title) {
    int id = big_app();
    char title[32]{};
    if (id >= 0 && app_title(uint32_t(id), title) == 0) {
      title[sizeof(title) - 1] = 0;
      std::snprintf(shell.active_title, sizeof(shell.active_title), "%s",
                    title);
    }
  }
}
static void frame(ps5::demo::Canvas &canvas) noexcept {
  bool was_browser = native_browser::active();
  native_browser::poll();
  if (native_browser::active())
    return;
  if (was_browser) {
    uint32_t user = 0;
    if (sceUserServiceGetForegroundUser(&user) == 0)
      native_input::open(int(user), log);
  }
  double now = monotonic();
  float elapsed = last_frame ? float(now - last_frame) : .016f;
  last_frame = now;
  if (now - last_status > 3) {
    refresh_status();
    last_status = now;
  }
  RawPad samples[16]{};
  int n = pad >= 0 ? scePadRead(pad, samples, 16) : 0;
  for (int i = 0; i < n; i++) {
    auto &r = samples[i];
    shell.controller_input(r.connected != 0, r.buttons, r.lx, r.ly, r.rx, r.ry,
                           i == n - 1 ? elapsed : 0.f);
    shell.touchpad_input(r.connected != 0 && !(r.buttons & 0x80000000u),
                         r.touch_data.fingers, r.touch_data.touch[0].x,
                         r.touch_data.touch[0].y, r.touch_data.touch[0].finger,
                         (r.buttons & 0x100000u) != 0, r.timestamp);
  }
  native_input::poll(shell, now);
  ww::dispatch_owner_check(shell, elevated, wine11_admission_observe);
  if (shell.pending == ww::Action::browser) {
    shell.pending = ww::Action::none;
    double request_started = monotonic();
    uint32_t browser_user = 0;
    int code = sceUserServiceGetForegroundUser(&browser_user);
    if (code == 0)
      code = native_browser::open(shell.url, int(browser_user));
    if (code == -1)
      code = sceSystemServiceLaunchWebBrowser(shell.url, nullptr);
    if (code == 0) shell.web_history.remember(shell.url);
    if (native_browser::active())
      native_input::close();
    log("system browser", uint32_t(code));
    log("browser request milliseconds",
        uint32_t((monotonic() - request_started) * 1000));
    if (code != 0)
      shell.result(false, uint32_t(code), "system browser");
  }
  if (shell.pending != ww::Action::none) {
    const char *title =
        shell.pending == ww::Action::quake ? "FAKE78003" : "FAKE77031";
    shell.pending = ww::Action::none;
    if (audio >= 0) {
      sceAudioOutClose(audio);
      audio = -1;
      shell.audio_available = false;
    }
    native_input::close();
    auto r = ww::launch_title(api, title, elevated);
    log(title, r.code);
    log(r.stage, r.code);
    shell.result(r.ok, r.code, r.stage);
    if (!r.ok) {
      open_audio();
      uint32_t user = 0;
      if (api.user && api.user(&user) == 0)
        native_input::open(int(user), log);
    }
  }
  if (shell.sound_test) {
    shell.sound_test = false;
    if (audio >= 0)
      test_sound();
  }
  shell.tick();
  shell.draw();
  canvas.blit(shell.pixels);
}
int main() {
  log("startup desktop update 16 static buffers", 0);
  int private_dir = open("/download0", O_RDONLY | O_DIRECTORY);
  if (!shell.init("/app0/assets", desktop_frame, desktop_background,
                  desktop_font, "/download0/desktop-settings.ini")) {
    log("assets missing", 1);
    for (;;)
      sceKernelUsleep(1000000);
  }
  log("assets loaded", 0);
  // System-module lookup depends on the original sandbox root. Load modules
  // only here; their service/device initialization follows the root grant.
  native_input::preload_modules(log);
  native_browser::preload_modules(log);
  // Elevation is requested once, before the UI starts; failure leaves the
  // desktop usable.
  auto elevation_status = elevation::request(elevation::Capability::filesystem);
  elevated = elevation_status == elevation::Status::ok;
  bool private_bound = bind_private_paths(private_dir);
  if (private_dir >= 0)
    close(private_dir);
  log("private directory rebound", private_bound ? 0 : 1);
  shell.load_web_history();
  log("root proof", elevated ? 0 : 1);
  log("filesystem request status", unsigned(elevation_status));
  log(elevation::path(), unsigned(elevation_status));
  // Service initialization may create threads; the helper requires one thread.
  // Modules have already loaded while their original sandbox paths existed.
  sceUserServiceInitialize(nullptr);
  native_browser::initialize_dialog();
  int user = -1;
  int user_status = sceUserServiceGetInitialUser(&user);
  if (user_status != 0) {
    uint32_t foreground = 0;
    user_status = sceUserServiceGetForegroundUser(&foreground);
    if (user_status == 0)
      user = int(foreground);
  }
  log("input user", unsigned(user_status));
  if (user_status == 0)
    native_input::open(user, log);
  scePadInit();
  if (user_status == 0)
    for (int i = 0; i < 20 && pad < 0; i++) {
      pad = scePadOpen(user, 0, 0, nullptr);
      if (pad < 0)
        sceKernelUsleep(50000);
    }
  log("controller", uint32_t(pad));
  sceSysmoduleLoadModuleInternal(0x80000010u);
  int32_t handle = -1;
  api.user = sceUserServiceGetForegroundUser;
  if (sceSysmoduleGetModuleHandleInternal(0x80000010u, &handle) == 0 &&
      handle >= 0) {
    void *init = nullptr, *launch = nullptr;
    auto a = sceKernelDlsym(handle, "sceLncUtilInitialize", &init);
    auto b = sceKernelDlsym(handle, "sceLncUtilLaunchApp", &launch);
    if (a == 0 && b == 0 && init && launch) {
      api.initialize = reinterpret_cast<decltype(api.initialize)>(init);
      api.launch = reinterpret_cast<decltype(api.launch)>(launch);
    } else {
      void *p = nullptr;
      if (sceKernelDlsym(handle, "sceSystemServiceLaunchApp", &p) == 0)
        api.public_launch = reinterpret_cast<decltype(api.public_launch)>(p);
    }
  }
  if (!api.launch && !api.public_launch)
    api.public_launch = sceSystemServiceLaunchApp;
  if (handle >= 0) {
    void *get = nullptr, *title = nullptr;
    if (sceKernelDlsym(handle, "sceLncUtilGetAppIdOfRunningBigApp", &get) == 0)
      big_app = reinterpret_cast<decltype(big_app)>(get);
    if (sceKernelDlsym(handle, "sceLncUtilGetAppTitleId", &title) == 0)
      app_title = reinterpret_cast<decltype(app_title)>(title);
  }
  sceSysmoduleLoadModuleInternal(0x80000009u);
  log("network init", uint32_t(sceNetCtlInit()));
  open_audio();
  ps5::demo::run_frames(frame, "WinBrew 11 desktop ready");
}
