// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "title_removal.hpp"
#include "uninstall_journal.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <functional>
#include <pthread.h>
namespace ww::steam_integration {
enum class UninstallFinish { pending, complete, failed };
// Caller-owned operations must outlive the flow. Each step returns true only
// after its durable effect has been independently verified.
class UninstallOperations {
 public:
  virtual ~UninstallOperations() = default;
  virtual bool Pause() = 0;
  // Reopens downloads only if no uninstall intent was published.
  virtual bool CancelEmpty() = 0;
  virtual bool Prepare(std::uint32_t, const std::string&, bool, std::uint64_t) = 0;
  virtual bool VerifyTitle(const TitleRemoval&) = 0;
  virtual bool RecordTitle(const TitleRemoval&) = 0;
  virtual bool Retire() = 0;
  virtual bool Reconcile() = 0;
  virtual bool Cleanup() = 0;
  virtual UninstallFinish Finish(bool reinstall, std::uint64_t account) = 0;
  virtual UninstallJournal::Entry Recovery() = 0;
  virtual bool RecoverTitle() = 0;
  virtual bool RecoverWork() = 0;
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  virtual bool ClaimOwnerRetireRecovery(const UninstallJournal::Entry&) = 0;
#endif
};
enum class UninstallFlowState { idle, title, working, finishing, complete, failed };
class UninstallFlow {
 public:
  UninstallFlow(UninstallOperations& operations, TitleRemoval& title);
  ~UninstallFlow();
  UninstallFlow(const UninstallFlow&) = delete;
  UninstallFlow& operator=(const UninstallFlow&) = delete;
  bool Begin(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account);
  bool Recover(); // false while the host is still starting
  std::optional<std::string> TitleFor(std::uint32_t app) const;
  void Tick();
  UninstallFlowState state() const { return state_; }
  const std::string& message() const { return message_; }
 private:
  UninstallOperations& operations_; TitleRemoval& title_; std::thread::id thread_;
  bool StartWorker(std::function<bool()> work);
  void JoinWorker();
  pthread_t worker_{}; bool worker_started_ = false;
  std::atomic<bool> worker_done_{false}; bool worker_ok_ = false;
  UninstallFlowState state_ = UninstallFlowState::idle;
  bool reinstall_ = false; std::uint64_t account_ = 0;
  std::string message_;
};
}
