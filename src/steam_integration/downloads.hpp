// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "installs/install_service.h"
#include "network_traffic.hpp"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
namespace ww::steam_integration {
// Serial presentation boundary for the Playden installation service. The
// service and its runner outlive this coordinator; the caller is responsible
// for proving that its game root is exclusive to this Wine 11 instance.
// No command or filesystem access occurs on the presentation thread.
class DownloadCoordinator {
 public:
  explicit DownloadCoordinator(playden::installs::InstallService&, traffic::Registry* = nullptr);
  std::shared_ptr<traffic::Counter> Traffic(std::uint32_t app) const;
  ~DownloadCoordinator();
  DownloadCoordinator(const DownloadCoordinator&) = delete;
  DownloadCoordinator& operator=(const DownloadCoordinator&) = delete;
  bool Install(std::uint32_t app);
  bool Pause(std::uint32_t app);
  bool Resume(std::uint32_t app);
  // Cancel removes Playden-managed partial data; call only after user action.
  bool Cancel(std::uint32_t app);
  bool DismissCompleted(std::uint32_t app);
  playden::installs::InstallSnapshot Snapshot() const;
  // Copy only a newly published worker view; commands and live safety checks
  // retain their existing semantics and do not use this presentation cursor.
  bool SnapshotIfChanged(playden::installs::InstallSnapshot&, std::uint64_t& generation) const;
  bool Busy() const;
  bool MaintenanceReady() const;
  bool SafeToExit() const;
  // Admission for an offline registry transaction. This freezes new commands
  // only when no active jobs, queued commands or worker effects remain.
  bool FreezeForMaintenance();
  bool ThawAfterMaintenance();
  bool FrozenForMaintenance() const;
  std::string Error() const;
 private:
  enum class Kind { install, pause, resume, cancel, dismiss };
  struct Command { Kind kind; std::uint32_t app; std::uint64_t sequence; };
  bool Submit(Kind, std::uint32_t);
  void Run();
  playden::installs::InstallService& service_;
  traffic::Registry* traffic_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<Command> commands_;
  playden::installs::InstallSnapshot snapshot_;
  bool stopping_ = false, in_flight_ = false, ready_ = false, failed_ = false, frozen_ = false;
  std::string error_;
  std::uint64_t sequence_ = 0, snapshot_generation_ = 0;
  std::thread worker_;
};
}
