// SPDX-License-Identifier: LGPL-2.1-or-later
// Module loading/opening and report ABI adapted from
// prospero-win/native/pw_hid_ps5.c. Copyright (c) 2026 prospero-win
// contributors; Wine 11 integration for KPKev.
#include "ps5_hid.hpp"
#include "hid_input.hpp"
#include "shell.hpp"
#include <cstdint>
#include <initializer_list>
extern "C" {
int sceSysmoduleLoadModule(uint16_t);
int sceSysmoduleIsLoaded(uint16_t);
int sceKeyboardInit();
int sceKeyboardOpen(int, int, int, void *);
int sceKeyboardClose(int);
int sceKeyboardReadState(int, void *);
int sceMouseInit();
int sceMouseOpen(int, int, int, void *);
int sceMouseClose(int);
int sceMouseRead(int, void *, int);
}
namespace native_input {
static ww::HidInput input;
static Log diagnostic = nullptr;
static int owner = 0;
static double retry_time = 0;
struct Device {
  uint16_t module_id;
  const char *name;
  int handle = -1;
  bool loaded = false;
  int (*init)() = nullptr;
  int (*open)(int, int, int, void *) = nullptr;
  int (*close)(int) = nullptr;
  int (*keyboard_read)(int, void *) = nullptr;
  int (*mouse_read)(int, void *, int) = nullptr;
  bool reported = false, connected = false;
};
static Device keyboard{0x0106, "keyboard"}, mouse{0x00a9, "mouse"};
static void log(const char *name, unsigned code) {
  if (diagnostic)
    diagnostic(name, code);
}
template <class Fn> static Fn linked(Fn address) {
  Fn volatile slot = address;
  return slot;
}
void preload_modules(Log logger) {
  diagnostic = logger;
  for (auto *d : {&keyboard, &mouse}) {
    if (d->loaded)
      continue;
    int status = sceSysmoduleLoadModule(d->module_id);
    d->loaded = status == 0 || sceSysmoduleIsLoaded(d->module_id) == 0;
    log(d == &keyboard ? "keyboard module preload" : "mouse module preload",
        unsigned(status));
    log(d == &keyboard ? "keyboard linked API ready" : "mouse linked API ready",
        d->loaded ? 0 : 1);
  }
  keyboard.init = linked(sceKeyboardInit);
  keyboard.open = linked(sceKeyboardOpen);
  keyboard.close = linked(sceKeyboardClose);
  keyboard.keyboard_read = linked(sceKeyboardReadState);
  mouse.init = linked(sceMouseInit);
  mouse.open = linked(sceMouseOpen);
  mouse.close = linked(sceMouseClose);
  mouse.mouse_read = linked(sceMouseRead);
  for (auto *d : {&keyboard, &mouse}) {
    unsigned mask = (d->init ? 1u : 0u) | (d->open ? 2u : 0u) |
                    (d->close ? 4u : 0u) |
                    ((d == &keyboard ? d->keyboard_read != nullptr
                                     : d->mouse_read != nullptr)
                         ? 8u
                         : 0u);
    log(d == &keyboard ? "keyboard linked exports" : "mouse linked exports",
        mask);
  }
}
static void open_device(Device &d) {
  if (d.handle >= 0)
    return;
  if (!d.loaded)
    return;
  if (d.init && d.open &&
      ((&d == &keyboard && d.keyboard_read) ||
       (&d == &mouse && d.mouse_read))) {
    int status = d.init();
    if (!d.reported)
      log(&d == &keyboard ? "keyboard init" : "mouse init", unsigned(status));
    uint8_t parameters[8]{};
    if (&d == &mouse)
      parameters[0] = 1;
    d.handle = d.open(owner, 0, 0, parameters);
    if (d.handle < 0 && parameters[0]) {
      parameters[0] = 0;
      d.handle = d.open(owner, 0, 0, parameters);
    }
  }
  if (d.handle >= 0 || !d.reported)
    log(&d == &keyboard ? "USB keyboard handle" : "USB mouse handle",
        unsigned(d.handle));
  d.reported = true;
}
void open(int user, Log logger) {
  owner = user;
  diagnostic = logger;
  open_device(keyboard);
  open_device(mouse);
}
void poll(ww::Shell &shell, double now) {
  if (now >= retry_time) {
    retry_time = now + 3;
    open_device(keyboard);
    open_device(mouse);
  }
  auto timestamp = uint64_t(now * 1000000);
  if (keyboard.handle >= 0) {
    uint8_t state[96]{};
    if (keyboard.keyboard_read(keyboard.handle, state) == 0)
      input.keyboard(shell, state, timestamp);
    if (shell.keyboard_connected != keyboard.connected) {
      keyboard.connected = shell.keyboard_connected;
      log("USB keyboard connected", keyboard.connected);
    }
  }
  if (mouse.handle >= 0) {
    uint8_t records[64 * 40]{};
    int count = mouse.mouse_read(mouse.handle, records, 64);
    if (count > 0)
      input.mouse(shell, records, count, timestamp);
    if (shell.mouse_connected != mouse.connected) {
      mouse.connected = shell.mouse_connected;
      log("USB mouse connected", mouse.connected);
    }
  }
}
void close() {
  for (auto *d : {&keyboard, &mouse}) {
    if (d->handle >= 0 && d->close)
      d->close(d->handle);
    d->handle = -1;
  }
  input = ww::HidInput{};
}
} // namespace native_input
