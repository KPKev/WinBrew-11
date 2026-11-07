#include "../src/steam_integration/uninstall_next_step.hpp"
#include <cassert>
using namespace ww::steam_integration;
int main() {
  using P = UninstallJournal::Phase; using O = UninstallObservation; using S = UninstallStep;
  UninstallJournal::Entry e{P::intent, 365670, "11111111-2222-4333-8444-555555555555", 1, 2, "FAKE78008", true};
  e.account = 101;
  UninstallFacts f; f.app_owned = f.account_unchanged = true;
  assert(NextUninstallStep(e, f) == S::wait);
  f.title = O::present; assert(NextUninstallStep(e, f) == S::remove_title);
  f.title = O::absent; assert(NextUninstallStep(e, f) == S::record_title_removed);
  e.phase = P::title_removed; assert(NextUninstallStep(e, f) == S::wait);
  f.files = O::present; assert(NextUninstallStep(e, f) == S::retire_game);
  f.files = O::absent; assert(NextUninstallStep(e, f) == S::blocked);
  f.retired_matches = true; assert(NextUninstallStep(e, f) == S::record_game_retired);
  e.phase = P::game_retired; f.registry = O::present; assert(NextUninstallStep(e, f) == S::reconcile_registry);
  f.registry = O::absent; assert(NextUninstallStep(e, f) == S::record_registry_removed);
  e.phase = P::registry_removed; f.quarantine = O::present; assert(NextUninstallStep(e, f) == S::remove_quarantine);
  f.quarantine = O::absent; assert(NextUninstallStep(e, f) == S::record_quarantine_removed);
  e.phase = P::quarantine_removed; f.download = O::absent; assert(NextUninstallStep(e, f) == S::record_queue_intent);
  e.phase = P::queue_intent; assert(NextUninstallStep(e, f) == S::queue_download);
  f.download = O::present; assert(NextUninstallStep(e, f) == S::blocked);
  f.matching_download = true; assert(NextUninstallStep(e, f) == S::record_download_queued);
  e.phase = P::download_queued; assert(NextUninstallStep(e, f) == S::finish);
  f.download = O::absent; assert(NextUninstallStep(e, f) == S::blocked);
  e.phase = P::queue_intent; f.download = O::present;
  f.account_unchanged = false; assert(NextUninstallStep(e, f) == S::blocked);
  f.account_unchanged = true; f.title = O::present; assert(NextUninstallStep(e, f) == S::blocked);
  e.reinstall = false; f.title = O::absent; assert(NextUninstallStep(e, f) == S::finish);
}

