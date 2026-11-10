// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/ps5_hid.hpp"
#include "../src/shell.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
static int opens = 0, closes = 0, reads = 0, loads = 0, inits = 0;
static bool elevated = false;
static int keyboard_status = 0;
static bool keyboard_present = true;
static uint16_t usage = 0;
static uint32_t modifiers = 0;
static int initialize() { assert(elevated); ++inits; return 0; }
static int open_device(int user, int, int, void *) {
  assert(user == 42);
  return 100 + opens++;
}
static int close_device(int) {
  ++closes;
  return 0;
}
static int key_read(int, void *data) {
  ++reads;
  auto *p = static_cast<unsigned char *>(data);
  p[16] = keyboard_present;
  int32_t count = usage ? 1 : 0;
  std::memcpy(p + 20, &count, 4);
  std::memcpy(p + 28, &modifiers, 4);
  std::memcpy(p + 32, &usage, 2);
  return keyboard_status;
}
static int mouse_read(int, void *data, int) {
  ++reads;
  auto *p = static_cast<unsigned char *>(data);
  p[8] = 1;
  uint64_t stamp = 123;
  std::memcpy(p, &stamp, 8);
  return 1;
}
extern "C" {
int sceSysmoduleLoadModule(uint16_t) {
  assert(!elevated); // Native loader returns 0x80020063 after the root grant.
  ++loads;
  return 0;
}
int sceSysmoduleIsLoaded(uint16_t) { return 0; }
int sceKeyboardInit() { return initialize(); }
int sceKeyboardOpen(int user, int type, int index, void *param) {
  return open_device(user, type, index, param);
}
int sceKeyboardClose(int handle) { return close_device(handle); }
int sceKeyboardReadState(int handle, void *data) {
  return key_read(handle, data);
}
int sceMouseInit() { return initialize(); }
int sceMouseOpen(int user, int type, int index, void *param) {
  return open_device(user, type, index, param);
}
int sceMouseClose(int handle) { return close_device(handle); }
int sceMouseRead(int handle, void *data, int count) {
  return mouse_read(handle, data, count);
}
}
static void log(const char *, unsigned) {}
int main() {
  ww::Shell s;
  assert(s.init("assets"));
  native_input::preload_modules(log);
  assert(loads == 2 && inits == 0 && opens == 0);
  elevated = true;
  // Repeated preparation must not attempt sandbox module lookup after root.
  native_input::preload_modules(log);
  assert(loads == 2 && inits == 0);
  native_input::open(42, log);
  assert(opens == 2);
  native_input::poll(s, 1);
  assert(reads == 2 && s.keyboard_connected && s.mouse_connected);
  s.activate(1421);
  usage = 4;
  native_input::poll(s, 1.1);
  assert(!std::strcmp(s.document.text, "a"));
  // A dead device must release modifiers/repeat state and recover without
  // restarting the app or disturbing the working mouse handle.
  modifiers = 1;
  native_input::poll(s, 1.2);
  assert(s.file_modifiers == 1);
  keyboard_status = -1;
  native_input::poll(s, 1.3);
  assert(!s.keyboard_connected && s.file_modifiers == 0 && s.mouse_connected);
  native_input::poll(s, 1.4);
  native_input::poll(s, 1.5);
  assert(closes == 1 && opens == 2);
  keyboard_status = 0; modifiers = 0;
  native_input::poll(s, 4.6);
  assert(opens == 3 && closes == 1 && s.keyboard_connected);
  assert(!std::strcmp(s.document.text, "aa"));
  // A receiver disconnect also retires its stale handle. Reconnection is
  // independent of the read-error path, and the same held key is usable.
  keyboard_present = false;
  native_input::poll(s, 4.7);
  assert(closes == 2 && !s.keyboard_connected && !s.file_modifiers);
  keyboard_present = true;
  native_input::poll(s, 7.8);
  assert(opens == 4 && s.keyboard_connected);
  assert(!std::strcmp(s.document.text, "aaa"));
  native_input::close();
  assert(closes == 4);
  native_input::open(42, log);
  assert(opens == 6);
  native_input::close();
  assert(closes == 6);
  s.activate(183);
  assert(s.pending == ww::Action::browser &&
         !std::strcmp(s.url, "http://example.com/"));
  std::puts("PASS: module load before root, device init/open after root, linked USB poll and "
            "close/reopen cleanly");
}
