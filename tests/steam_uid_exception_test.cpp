// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <functional>
#include <stdexcept>
#include <sys/types.h>
static uid_t simulated_uid = 1000;
static bool refuse_restore = false;
extern "C" uid_t fake_geteuid() { return simulated_uid; }
extern "C" int fake_seteuid(uid_t uid) {
  if (uid == 1000 && refuse_restore) return -1;
  simulated_uid = uid; return 0;
}
namespace playden::ps5 { bool WithRoot(const std::function<bool()>&); }
int main() {
  using playden::ps5::WithRoot;
  assert(WithRoot([] { assert(simulated_uid == 0); return true; }));
  assert(simulated_uid == 1000);
  assert(!WithRoot([] { return false; }) && simulated_uid == 1000);
  bool caught = false;
  try { WithRoot([]() -> bool { throw std::logic_error("original"); }); }
  catch (const std::logic_error&) { caught = true; }
  assert(caught && simulated_uid == 1000);
  refuse_restore = true;
  assert(!WithRoot([] { return true; }) && simulated_uid == 0);
  simulated_uid = 1000; caught = false;
  try { WithRoot([]() -> bool { throw std::logic_error("original"); }); }
  catch (const std::runtime_error&) { caught = true; }
  assert(caught && simulated_uid == 0); // Explicitly unverified, not recovery.
}
