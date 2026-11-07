// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "uninstall_journal.hpp"
namespace ww::steam_integration {
// Pure decision layer. Observations must come from authoritative, current
// snapshots; unknown or an in-flight operation is never treated as absence.
enum class UninstallObservation { unknown, present, absent, busy, failed };
enum class UninstallStep { blocked, wait, remove_title, record_title_removed,
  retire_game, record_game_retired, reconcile_registry, record_registry_removed, remove_quarantine, record_quarantine_removed,
  record_queue_intent, queue_download, record_download_queued, finish };
struct UninstallFacts {
  // Absent requires both Playden's runtime record and the console inventory
  // to confirm absence; a missing/inaccessible record is not proof.
  UninstallObservation title = UninstallObservation::unknown;
  UninstallObservation files = UninstallObservation::unknown;
  UninstallObservation registry = UninstallObservation::unknown;
  UninstallObservation quarantine = UninstallObservation::unknown;
  UninstallObservation download = UninstallObservation::unknown;
  bool app_owned = false;
  bool account_unchanged = false;
  // An absent original path only counts after the same recorded inode has
  // been found and synced in the operation-specific quarantine.
  bool retired_matches = false;
  // True only when durable evidence binds the observed download to this
  // exact reinstall operation; a matching app ID alone is insufficient.
  bool matching_download = false;
};
inline UninstallStep NextUninstallStep(const UninstallJournal::Entry& e, const UninstallFacts& f) {
  using P = UninstallJournal::Phase;
  using O = UninstallObservation;
  using S = UninstallStep;
  const auto indeterminate = [](O o) { return o == O::unknown || o == O::busy; };
  if (e.phase == P::blocked || e.phase == P::absent || !e.app || e.operation.empty()) return S::blocked;
  if (f.title == O::failed || f.files == O::failed || f.registry == O::failed || f.quarantine == O::failed || f.download == O::failed)
    return S::blocked;
  if (indeterminate(f.title)) return S::wait;
  if (e.phase == P::intent)
    return f.title == O::present ? S::remove_title : S::record_title_removed;
  if (f.title != O::absent) return S::blocked;  // Never create a second dashboard entry.
  if (indeterminate(f.files)) return S::wait;
  if (e.phase == P::title_removed)
    return f.files == O::present ? S::retire_game : f.retired_matches ? S::record_game_retired : S::blocked;
  if (e.phase <= P::registry_removed && f.files != O::absent) return S::blocked;
  if (e.phase <= P::game_retired && !f.retired_matches) return S::blocked;
  if (indeterminate(f.registry)) return S::wait;
  if (e.phase == P::game_retired)
    return f.registry == O::present ? S::reconcile_registry : S::record_registry_removed;
  if (f.registry != O::absent) return S::blocked;
  if (indeterminate(f.quarantine)) return S::wait;
  if (e.phase == P::registry_removed) {
    if (f.quarantine == O::present) return f.retired_matches ? S::remove_quarantine : S::blocked;
    return S::record_quarantine_removed;
  }
  if (f.quarantine != O::absent) return S::blocked;
  if (!e.reinstall) return S::finish;
  if (!f.account_unchanged || !f.app_owned) return S::blocked;
  if (e.phase == P::quarantine_removed) return S::record_queue_intent;
  if (indeterminate(f.download)) return S::wait;
  if (f.download == O::present && !f.matching_download) return S::blocked;
  if (e.phase == P::download_queued) return f.download == O::present ? S::finish : S::blocked;
  if (f.files == O::present && (f.download != O::present || !f.matching_download)) return S::blocked;
  return f.download == O::present ? S::record_download_queued : S::queue_download;
}
}  // namespace ww::steam_integration

