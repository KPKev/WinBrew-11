// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/ps5_browser.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace native_browser {
struct Param;
}
static int common_calls = 0, init_calls = 0, closes = 0, terms = 0, status = 2,
           opens = 0;
static const char *saved = nullptr;
static bool elevated = false;
static int loads = 0;
static int common() {
  assert(elevated);
  ++common_calls;
  return 0;
}
static int init() {
  assert(common_calls == 1);
  ++init_calls;
  return 0;
}
template <class T> T at(const void *p, size_t offset) {
  T value{};
  std::memcpy(&value, static_cast<const uint8_t *>(p) + offset, sizeof(value));
  return value;
}
static int open_browser(native_browser::Param *p) {
  assert(at<uint64_t>(p, 0) == 48 && at<uint64_t>(p, 48) == 328);
  assert(at<int32_t>(p, 56) == 1 && at<int32_t>(p, 60) == 42);
  assert(at<uint32_t>(p, 44) == uint32_t(0xc0d1a109u + uintptr_t(p)));
  saved = at<const char *>(p, 64);
  assert(!std::strcmp(saved, "https://www.google.com"));
  ++opens;
  return 0;
}
static int update() { return status; }
static int close_dialog() {
  ++closes;
  return 0;
}
static int term() {
  ++terms;
  return 0;
}
extern "C" {
int sceSysmoduleLoadModule(uint16_t id) {
  assert(id == 0xab && !elevated);
  ++loads;
  return 0;
}
int sceSysmoduleIsLoaded(uint16_t) { return 0; }
int sceCommonDialogInitialize() { return common(); }
int sceWebBrowserDialogInitialize() { return init(); }
int sceWebBrowserDialogOpen(native_browser::Param *p) {
  return open_browser(p);
}
int sceWebBrowserDialogUpdateStatus() { return update(); }
int sceWebBrowserDialogClose() { return close_dialog(); }
int sceWebBrowserDialogTerminate() { return term(); }
}
static void log(const char *, unsigned) {}
int main() {
  assert(native_browser::open("https://www.google.com", 42) == -1);
  native_browser::preload_modules(log);
  assert(loads == 1 && !common_calls && !init_calls);
  assert(native_browser::open("https://www.google.com", 42) == -1);
  elevated = true;
  native_browser::preload_modules(log);
  assert(loads == 1 && !common_calls && !init_calls);
  native_browser::initialize_dialog();
  native_browser::initialize_dialog();
  assert(init_calls == 1 && common_calls == 1);
  char url[128] = "https://www.google.com";
  assert(native_browser::open(url, 42) == 0);
  url[0] = 'X';
  assert(!std::strcmp(saved, "https://www.google.com"));
  assert(native_browser::active());
  assert(native_browser::open(url, 42) == -2 && opens == 1);
  native_browser::poll();
  assert(native_browser::active());
  status = 3;
  native_browser::poll();
  assert(!native_browser::active() && closes == 1);
  native_browser::shutdown();
  assert(terms == 1);
  std::puts("PASS: browser module before root, init after root, parameter ABI, persistent "
            "URL, duplicate-open guard and finished lifecycle");
}
