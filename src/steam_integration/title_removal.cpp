// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_removal.hpp"
namespace ww::steam_integration {
using namespace playden;
using namespace playden::homebrew;
bool TitleRemoval::HostReady() const {
  if (std::this_thread::get_id() != thread_) return false;
  const auto snapshot = host_.Snapshot();
  return snapshot && snapshot->status == RuntimeHostStatus::kReady && snapshot->installs_known && !snapshot->removing_app;
}
std::optional<std::string> TitleRemoval::TitleFor(std::uint32_t app) const {
  if (std::this_thread::get_id() != thread_ || state_ == TitleRemovalState::waiting || !app) return {};
  const auto snapshot = host_.Snapshot();
  if (!snapshot || snapshot->status != RuntimeHostStatus::kReady || !snapshot->installs_known || snapshot->removing_app) return {};
  const std::string expected_root = "/data/wine11/wine11-games/" + std::to_string(app);
  std::optional<std::string> result;
  for (const auto& record : snapshot->installs) {
    if (record.runtime_number != app && record.game != GameId::Steam(app)) continue;
    if (result || record.runtime_number != app || record.game != GameId::Steam(app) || record.moving ||
        record.install_path != expected_root || record.title_id.empty() ||
        record.state == playden::runtime::PreparationState::kRemoving) return {};
    result = record.title_id;
  }
  return result;
}
bool TitleRemoval::CanBegin(std::uint32_t app, const std::string& title) const {
  const auto actual = TitleFor(app); return actual && *actual == title;
}
bool TitleRemoval::Begin(std::uint32_t app, const std::string& title) {
  if (!CanBegin(app, title)) return false;
  const auto snapshot = host_.Snapshot();
  if (!snapshot || snapshot->status != RuntimeHostStatus::kReady || !snapshot->installs_known) return false;
  app_ = app; title_ = title; before_ = snapshot->removals_handled; message_.clear();
  state_ = TitleRemovalState::waiting; host_.Remove(app); return true;
}
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
bool TitleRemoval::CanRetryFailedRetire(std::uint32_t app, const std::string& title) const {
  if (std::this_thread::get_id() != thread_ || state_ == TitleRemovalState::waiting ||
      app != 365670 || title != "FAKE78008") return false;
  const auto snapshot = host_.Snapshot();
  if (!snapshot || snapshot->status != RuntimeHostStatus::kReady || !snapshot->installs_known || snapshot->removing_app) return false;
  unsigned matches = 0;
  for (const auto& record : snapshot->installs) {
    if (record.runtime_number != app && record.game != GameId::Steam(app) && record.title_id != title) continue;
    if (++matches != 1 || record.runtime_number != app || record.game != GameId::Steam(app) ||
        record.title_id != title || record.install_path != "/data/wine11/wine11-games/365670" || record.moving ||
        record.state != playden::runtime::PreparationState::kRemoving || record.step != "retire" ||
        record.failure_code != "retire-failed") return false;
  }
  return matches == 1;
}
bool TitleRemoval::BeginRetryFailedRetire(std::uint32_t app, const std::string& title) {
  if (!CanRetryFailedRetire(app, title)) return false;
  const auto snapshot = host_.Snapshot();
  if (!snapshot || !CanRetryFailedRetire(app, title)) return false;
  app_ = app; title_ = title; before_ = snapshot->removals_handled; message_.clear();
  state_ = TitleRemovalState::waiting; host_.Remove(app); return true;
}
#endif
void TitleRemoval::Tick() {
  if (std::this_thread::get_id() != thread_ || state_ != TitleRemovalState::waiting) return;
  const auto snapshot = host_.Snapshot();
  if (!snapshot || snapshot->status != RuntimeHostStatus::kReady || !snapshot->installs_known) return;
  if (snapshot->removing_app && *snapshot->removing_app == app_) return;
  if (snapshot->removals_handled <= before_) return;
  const auto refusal = snapshot->removal_failures.find(app_);
  if (refusal != snapshot->removal_failures.end()) {
    state_ = TitleRemovalState::failed; message_ = refusal->second; return;
  }
  for (const auto& record : snapshot->installs) {
    if (record.runtime_number == app_ || record.title_id == title_) return;
  }
  state_ = TitleRemovalState::removed;
}
}
