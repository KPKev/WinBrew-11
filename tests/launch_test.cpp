// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/title_launch.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
static int calls = 0;
static uint32_t code = 0;
static uint32_t flags = 0;
static bool userfail = false;
static int init() {
  calls++;
  return 0;
}
static int user(uint32_t *u) {
  calls++;
  *u = 42;
  return userfail ? int(0x80000001) : 0;
}
static uint32_t launch(const char *t, const char **argv,
                       ww::LaunchParameters *p) {
  calls++;
  assert(!std::strcmp(t, "FAKE78003"));
  assert(argv == nullptr);
  assert(p->user == 42 && p->size == 32);
  flags = p->check_flag;
  return code;
}
static uint32_t pub(const char *t, const char **v, void *p) {
  return launch(t, v, static_cast<ww::LaunchParameters *>(p));
}
int main() {
  ww::TitleApi a{init, user, launch, nullptr};
  assert(!ww::launch_title(a, "FAKE78003", false).ok && calls == 0);
  assert(ww::launch_title(a, "FAKE78003", true).ok && calls == 3 && flags == 0);
  code = 0x80940031;
  auto r = ww::launch_title(a, "FAKE78003", true);
  assert(!r.ok && r.code == code);
  userfail = true;
  calls = 0;
  r = ww::launch_title(a, "FAKE78003", true);
  assert(!r.ok && calls == 2);
  userfail = false;
  code = 7;
  a.launch = nullptr;
  a.public_launch = pub;
  calls = 0;
  assert(ww::launch_title(a, "FAKE78003", true).ok && calls == 2 && flags == 1);
  a.user = nullptr;
  assert(!ww::launch_title(a, "FAKE78003", true).ok);
  std::puts("PASS: native/public handoff ABI, privilege gate, user failure, "
            "error propagation, accepted nonzero result");
}
