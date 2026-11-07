// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/runtime_update/coordinator.hpp"
#include <cassert>
using namespace ww::runtime_update;
struct Fake : Backend {
  int checks = 0, installs = 0;
  CheckResult check_result = CheckResult::available;
  InstallResult install_result = InstallResult::installed;
  Offer next = {"signed-release-1", "11.1"};
  CheckResult check(Offer &out) override { ++checks; out = next; return check_result; }
  InstallResult install(const Offer &o) override { ++installs; assert(o.release_id == next.release_id); return install_result; }
};
int main() {
  Coordinator disabled(nullptr); disabled.tick(0); disabled.check_now(1);
  assert(disabled.state() == State::unsupported && !disabled.install_approved("x"));
  Fake f; Coordinator c(&f, 10);
  c.tick(100); assert(f.checks == 1 && c.state() == State::available);
  c.tick(109); assert(f.checks == 1 && f.installs == 0);
  assert(!c.install_approved("different") && f.installs == 0);
  c.tick(110); assert(f.checks == 2);
  assert(c.install_approved("signed-release-1") && f.installs == 1);
  assert(c.state() == State::restart_required && !c.install_approved("signed-release-1"));
  c.tick(500); assert(f.checks == 2);
  Fake bad; bad.next.release_id.clear(); Coordinator invalid(&bad);
  invalid.tick(0); assert(invalid.state() == State::check_failed);
  Fake unavailable; unavailable.check_result = CheckResult::unavailable;
  Coordinator offline(&unavailable); offline.tick(3); assert(offline.state() == State::check_failed);
  offline.tick(2); assert(unavailable.checks == 2);
  Fake failed; failed.install_result = InstallResult::failed_restored;
  Coordinator rollback(&failed); rollback.tick(1); rollback.install_approved(failed.next.release_id);
  assert(rollback.state() == State::install_failed && !rollback.install_approved(failed.next.release_id));
  Fake danger; danger.install_result = InstallResult::recovery_required;
  Coordinator recovery(&danger); recovery.tick(1); recovery.install_approved(danger.next.release_id);
  recovery.check_now(2); assert(recovery.state() == State::recovery_required && danger.checks == 1);
}
