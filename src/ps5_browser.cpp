// SPDX-License-Identifier: GPL-3.0-or-later
// Browser dialog ABI checked against EVO-PLAYER-PS5 evo_webui.c at
// 9ee9a5420c73b5be448610e5f0a90a29a75592f3 (GPL-3.0-or-later).
#include "ps5_browser.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace native_browser {
struct Param;
}
extern "C" {
int sceSysmoduleLoadModule(uint16_t);
int sceSysmoduleIsLoaded(uint16_t);
int sceCommonDialogInitialize();
int sceWebBrowserDialogInitialize();
int sceWebBrowserDialogOpen(native_browser::Param *);
int sceWebBrowserDialogUpdateStatus();
int sceWebBrowserDialogClose();
int sceWebBrowserDialogTerminate();
}
namespace native_browser {
struct Base {
  uint64_t size;
  uint8_t reserved[36];
  uint32_t magic;
};
struct Param {
  Base base;
  uint64_t size;
  int32_t mode, user;
  const char *url;
  void *callback;
  uint16_t width, height, x, y;
  uint32_t parts;
  uint16_t header_width, header_x, header_y, pad;
  uint32_t control;
  void *ime, *webview;
  uint32_t animation;
  uint8_t reserved[202];
  uint16_t tail;
};
static_assert(sizeof(Base) == 48 && sizeof(Param) == 328);
static_assert(offsetof(Param, url) == 64 && offsetof(Param, control) == 100 &&
              offsetof(Param, animation) == 120);
static Param parameter{};
static char address[768]{};
static Log diagnostic = nullptr;
static int (*initialize)() = nullptr, (*common_init)() = nullptr,
           (*update)() = nullptr, (*close_dialog)() = nullptr,
           (*terminate)() = nullptr;
static int (*open_dialog)(Param *) = nullptr;
template <class Fn> static Fn linked(Fn address) {
  Fn volatile slot = address;
  return slot;
}
static bool loaded = false, ready = false, showing = false;
static void log(const char *label, int code) {
  if (diagnostic)
    diagnostic(label, unsigned(code));
}
void preload_modules(Log logger) {
  diagnostic = logger;
  if (loaded)
    return;
  int rc = sceSysmoduleLoadModule(0xab);
  log("browser dialog module", rc);
  if (rc != 0 && sceSysmoduleIsLoaded(0xab) != 0)
    return;
  common_init = linked(sceCommonDialogInitialize);
  initialize = linked(sceWebBrowserDialogInitialize);
  open_dialog = linked(sceWebBrowserDialogOpen);
  update = linked(sceWebBrowserDialogUpdateStatus);
  close_dialog = linked(sceWebBrowserDialogClose);
  terminate = linked(sceWebBrowserDialogTerminate);
  unsigned mask = (common_init ? 1u : 0u) | (initialize ? 2u : 0u) |
                  (open_dialog ? 4u : 0u) | (update ? 8u : 0u) |
                  (close_dialog ? 16u : 0u) | (terminate ? 32u : 0u);
  log("browser linked exports", int(mask));
  loaded = mask == 63;
}
void initialize_dialog() {
  if (!loaded || ready)
    return;
  log("common dialog init", common_init());
  int rc = initialize();
  log("browser dialog init", rc);
  ready = rc == 0;
}

int open(const char *url, int user) {
  if (!ready)
    return -1;
  if (showing)
    return -2;
  size_t length = std::strlen(url);
  if (length >= sizeof(address) || user < 0)
    return -3;
  std::memcpy(address, url, length + 1);
  parameter = {};
  parameter.base.size = sizeof(Base);
  parameter.base.magic = uint32_t(0xc0d1a109u + uintptr_t(&parameter.base));
  parameter.size = sizeof(Param);
  parameter.mode = 1;
  parameter.user = user;
  parameter.url = address;
  int rc = open_dialog(&parameter);
  log("browser dialog open", rc);
  showing = rc == 0;
  return rc;
}
bool active() { return showing; }
void poll() {
  if (!showing)
    return;
  int status = update();
  if (status != 2) {
    log("browser dialog status", status);
    showing = false;
    if (status == 3)
      close_dialog();
  }
}
void shutdown() {
  if (showing)
    close_dialog();
  showing = false;
  if (ready)
    terminate();
  ready = false;
}
} // namespace native_browser
