// SPDX-License-Identifier: GPL-3.0-or-later
// Runs Vapor the way Playden's backend expects: with the app user's effective
// UID, raising root only through playden::ps5::WithRoot.
//
// Call once on the application thread, right after the filesystem grant and
// before any Steam/Playden service or worker thread starts. The grant leaves
// the real UID at 0, so root can still be raised later for specific steps.
#pragma once
#include "../ownership_adopt.hpp"
#include <sys/types.h>

namespace ww::steam_integration {
enum class AppUserStage {
  lowered,           // running as the app user (adoption done now or earlier)
  not_elevated,      // no grant: nothing to adopt or lower
  original_was_root, // the title started as root; keep the old behaviour
  no_real_root,      // the grant left no real root to raise later; keep root
  adopt_incomplete,  // the read-only plan found something unexpected: nothing changed, stay root, retry next start
  lower_failed,      // seteuid refused or did not take effect; still root
};
struct AppUserOutcome {
  AppUserStage stage = AppUserStage::not_elevated;
  uid_t app_uid = 0;
  bool adopted_now = false;  // the one-time conversion ran on this start
  int error = 0;
  ww::ownership::Stats stats;
};
const char *AppUserStageName(AppUserStage stage);

struct AppUserPaths {
  const char *private_root;  // the app's private directory (Vapor state, settings, log)
  const char *data_root = "/data";
};

// original_euid: geteuid() recorded before the grant was requested.
AppUserOutcome AdoptAndLower(uid_t original_euid, bool elevated, const AppUserPaths &paths);
}  // namespace ww::steam_integration
