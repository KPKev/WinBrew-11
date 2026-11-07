// SPDX-License-Identifier: GPL-3.0-or-later
#include "downloads.hpp"
#include <chrono>
#include <utility>
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
extern "C" { [[gnu::used]] int wine11_cancel_cleanup_coordinator_enabled = 1; }
#endif
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
extern "C" { [[gnu::used]] int wine11_cancel_guard_coordinator_enabled = 1; }
#endif
namespace ww::steam_integration {
DownloadCoordinator::DownloadCoordinator(playden::installs::InstallService& service, traffic::Registry* traffic)
    : service_(service), traffic_(traffic), worker_([this] { Run(); }) {}
DownloadCoordinator::~DownloadCoordinator() {
  { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
  wake_.notify_one();
  worker_.join();
}
bool DownloadCoordinator::Submit(Kind kind, std::uint32_t app) {
  if (!app) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || failed_ || frozen_ || commands_.size() >= 64) return false;
  error_.clear(); commands_.push_back({kind, app, ++sequence_}); wake_.notify_one(); return true;
}
std::shared_ptr<traffic::Counter> DownloadCoordinator::Traffic(std::uint32_t app) const {
  return traffic_ ? traffic_->get(app) : nullptr;
}
bool DownloadCoordinator::Install(std::uint32_t app) { return Submit(Kind::install, app); }
bool DownloadCoordinator::Pause(std::uint32_t app) { return Submit(Kind::pause, app); }
bool DownloadCoordinator::Resume(std::uint32_t app) { return Submit(Kind::resume, app); }
bool DownloadCoordinator::Cancel(std::uint32_t app) {
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
  if (!app) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || failed_) return false;
  error_ = "Cancellation is unavailable. Pause the download to keep its files.";
  return false;
#else
  return Submit(Kind::cancel, app);
#endif
}
bool DownloadCoordinator::DismissCompleted(std::uint32_t app) { return Submit(Kind::dismiss, app); }
playden::installs::InstallSnapshot DownloadCoordinator::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_); return snapshot_;
}
bool DownloadCoordinator::SnapshotIfChanged(playden::installs::InstallSnapshot& out, std::uint64_t& generation) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (generation == snapshot_generation_) return false;
  out = snapshot_; generation = snapshot_generation_; return true;
}
bool DownloadCoordinator::Busy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (failed_ || frozen_ || !ready_ || in_flight_ || !commands_.empty() || !snapshot_.jobs.empty()) return true;
#if defined(WINE11_SERVICE_QUIESCENCE)
  if (!service_.WorkerIdleForExit()) return true;
#endif
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  if (snapshot_.recovery_blocked) return true;
  if (!snapshot_.cancel_cleanup.empty() || !snapshot_.cancel_cleanup_failures.empty()) return true;
#endif
  return false;
}
bool DownloadCoordinator::SafeToExit() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || failed_ || frozen_ || !ready_ || in_flight_ || !commands_.empty()) return false;
#if defined(WINE11_SERVICE_QUIESCENCE)
  // Lock order: the service worker never holds its mutex while acquiring this
  // coordinator mutex. Holding this lock also excludes new UI commands.
  return service_.WorkerIdleForExit();
#else
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  if (snapshot_.recovery_blocked) return false;
  if (!snapshot_.cancel_cleanup.empty() || !snapshot_.cancel_cleanup_failures.empty()) return false;
#endif
  for (const auto& job : snapshot_.jobs)
    if (job.state == playden::installs::InstallJobState::kQueued ||
        job.state == playden::installs::InstallJobState::kRunning) return false;
  return true;
#endif
}
bool DownloadCoordinator::MaintenanceReady() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || failed_ || frozen_ || !ready_ || in_flight_ || !commands_.empty() ||
      !snapshot_.uninstalling.empty()) return false;
  for (const auto& job : snapshot_.jobs)
    if (job.state != playden::installs::InstallJobState::kPaused) return false;
#if defined(WINE11_SERVICE_QUIESCENCE)
  return service_.WorkerIdleForExit();
#else
  return false;
#endif
}
bool DownloadCoordinator::FreezeForMaintenance() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (frozen_ || stopping_ || failed_ || !ready_ || in_flight_ || !commands_.empty() ||
      !snapshot_.uninstalling.empty()) return false;
  for (const auto& job : snapshot_.jobs)
    if (job.state != playden::installs::InstallJobState::kPaused) return false;
#if defined(WINE11_SERVICE_QUIESCENCE)
  if (!service_.WorkerIdleForExit()) return false;
  frozen_ = true; return true;
#else
  return false;
#endif
}
bool DownloadCoordinator::ThawAfterMaintenance() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!frozen_ || stopping_ || failed_) return false;
  frozen_ = false; return true;
}
bool DownloadCoordinator::FrozenForMaintenance() const {
  std::lock_guard<std::mutex> lock(mutex_); return frozen_ && !stopping_ && !failed_;
}
std::string DownloadCoordinator::Error() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!error_.empty()) return error_;
#if defined(WINE11_CANCEL_CLEANUP_OVERLAY)
  if (snapshot_.recovery_blocked) return "Download recovery is unavailable. No downloads were started.";
  if (!snapshot_.cancel_cleanup_failures.empty()) return snapshot_.cancel_cleanup_failures.front().reason;
#endif
  return {};
}
void DownloadCoordinator::Run() {
  for (;;) {
    Command command{}; bool has_command = false;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait_for(lock, std::chrono::milliseconds(250), [this] { return stopping_ || !commands_.empty(); });
      if (stopping_ && commands_.empty()) return;
      if (!commands_.empty()) { command = commands_.front(); commands_.pop_front(); in_flight_ = true; has_command = true; }
    }
    try {
    if (has_command) {
      auto id = playden::GameId::Steam(command.app);
      switch (command.kind) {
      case Kind::install: service_.Install(id); break;
      case Kind::pause: service_.Pause(id); break;
      case Kind::resume: service_.Resume(id); break;
      case Kind::cancel: service_.Cancel(id); break;
      case Kind::dismiss: service_.DismissCompleted(id); break;
      }
    }
    auto current = service_.Snapshot();
    bool rejected = false;
    if (has_command && command.kind == Kind::install) {
      const auto id = playden::GameId::Steam(command.app);
      bool found = false;
      for (const auto& job : current.jobs) if (job.id == id) found = true;
      for (const auto& game : current.installed) if (game.id == id) found = true;
      if (!found) rejected = true;
    }
    { std::lock_guard<std::mutex> lock(mutex_);
      snapshot_ = std::move(current); ++snapshot_generation_; in_flight_ = false; ready_ = true;
      if (rejected && command.sequence == sequence_) error_ = "The download could not be queued.";
    }
    } catch (...) {
      std::lock_guard<std::mutex> lock(mutex_);
      failed_ = true; in_flight_ = false;
      error_ = "Download service stopped unexpectedly. Restart before changing accounts.";
      return;
    }
  }
}
}
