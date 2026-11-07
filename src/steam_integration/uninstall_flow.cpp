// SPDX-License-Identifier: GPL-3.0-or-later
#include "uninstall_flow.hpp"
namespace ww::steam_integration {
UninstallFlow::UninstallFlow(UninstallOperations& operations, TitleRemoval& title)
    : operations_(operations), title_(title), thread_(std::this_thread::get_id()) {}
UninstallFlow::~UninstallFlow() { JoinWorker(); }
void UninstallFlow::JoinWorker() {
  if (worker_started_) { pthread_join(worker_, nullptr); worker_started_ = false; }
}
bool UninstallFlow::StartWorker(std::function<bool()> work) {
  struct Task { UninstallFlow* flow; std::function<bool()> work; };
  auto task = std::make_unique<Task>(Task{this, std::move(work)});
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) return false;
  worker_done_ = false; worker_ok_ = false;
  Task* raw = task.release();
  const bool ok = pthread_attr_setstacksize(&attr, 1024 * 1024) == 0 &&
      pthread_create(&worker_, &attr, [](void* arg) -> void* {
        std::unique_ptr<Task> task(static_cast<Task*>(arg));
        try { task->flow->worker_ok_ = task->work(); } catch (...) { task->flow->worker_ok_ = false; }
        task->flow->worker_done_.store(true, std::memory_order_release);
        return nullptr;
      }, raw) == 0;
  pthread_attr_destroy(&attr);
  if (ok) worker_started_ = true; else delete raw;
  return ok;
}
std::optional<std::string> UninstallFlow::TitleFor(std::uint32_t app) const {
  if (state_ != UninstallFlowState::idle && state_ != UninstallFlowState::complete) return {};
  return title_.TitleFor(app);
}
bool UninstallFlow::Recover() {
  if (std::this_thread::get_id() != thread_ || state_ != UninstallFlowState::idle || !title_.HostReady()) return false;
  const auto entry = operations_.Recovery();
  if (entry.phase == UninstallJournal::Phase::absent) return false;
  if (entry.phase == UninstallJournal::Phase::blocked) {
    state_ = UninstallFlowState::failed; message_ = "The previous removal record cannot be verified."; return false;
  }
  reinstall_ = entry.reinstall; account_ = entry.account;
#if defined(WINE11_OWNER_RETIRE_RECOVERY)
  const bool owner_target = entry.phase == UninstallJournal::Phase::intent && entry.app == 365670 &&
      entry.title == "FAKE78008" && !entry.reinstall;
  if (owner_target && title_.CanRetryFailedRetire(entry.app, entry.title)) {
    if (!operations_.ClaimOwnerRetireRecovery(entry) || !title_.BeginRetryFailedRetire(entry.app, entry.title)) {
      state_ = UninstallFlowState::failed; message_ = "The saved PS5 title recovery could not be verified."; return false;
    }
    state_ = UninstallFlowState::title; message_ = "Resuming PS5 title removal..."; return true;
  }
  if (!owner_target && entry.phase == UninstallJournal::Phase::intent && title_.CanBegin(entry.app, entry.title)) {
#else
  if (entry.phase == UninstallJournal::Phase::intent && title_.CanBegin(entry.app, entry.title)) {
#endif
    if (!title_.Begin(entry.app, entry.title)) return false;
    state_ = UninstallFlowState::title; message_ = "Resuming PS5 title removal..."; return true;
  }
  try {
    if (!StartWorker([this, phase = entry.phase] {
      return (phase != UninstallJournal::Phase::intent || operations_.RecoverTitle()) && operations_.RecoverWork();
    })) throw std::bad_alloc();
  } catch (...) { state_ = UninstallFlowState::failed; message_ = "Could not start removal recovery."; return false; }
  state_ = UninstallFlowState::working; message_ = "Recovering a previous game removal..."; return true;
}
bool UninstallFlow::Begin(std::uint32_t app, const std::string& title, bool reinstall, std::uint64_t account) {
  if (std::this_thread::get_id() != thread_ || (state_ != UninstallFlowState::idle && state_ != UninstallFlowState::complete) || !app || title.empty() || (reinstall && !account)) return false;
  if (!title_.CanBegin(app, title)) return false;
  reinstall_ = reinstall; account_ = account;
  if (!operations_.Pause()) { message_ = "Wait for other downloads and game activity to finish."; return false; }
  if (!operations_.Prepare(app, title, reinstall, account)) {
    const bool clean = operations_.CancelEmpty();
    state_ = clean ? UninstallFlowState::idle : UninstallFlowState::failed;
    message_ = clean ? "Game removal could not start." : "Game removal needs recovery before downloads can continue.";
    return false;
  }
  if (!title_.Begin(app, title)) {
    state_ = UninstallFlowState::failed; message_ = "Game removal could not start. Restart Vapor to recover."; return false;
  }
  state_ = UninstallFlowState::title; message_ = "Removing the PS5 Home entry..."; return true;
}
void UninstallFlow::Tick() {
  if (std::this_thread::get_id() != thread_) return;
  if (state_ == UninstallFlowState::title) {
    title_.Tick();
    if (title_.state() == TitleRemovalState::failed) {
      state_ = UninstallFlowState::failed; message_ = title_.message(); return;
    }
    if (title_.state() != TitleRemovalState::removed) return;
    try {
      if (!StartWorker([this] {
        return operations_.VerifyTitle(title_) && operations_.RecordTitle(title_) &&
               operations_.Retire() && operations_.Reconcile() && operations_.Cleanup();
      })) throw std::bad_alloc();
    } catch (...) { state_ = UninstallFlowState::failed; message_ = "Could not start game cleanup."; return; }
    state_ = UninstallFlowState::working; message_ = "Removing game files...";
  }
  if (state_ == UninstallFlowState::working && worker_done_.load(std::memory_order_acquire)) {
    JoinWorker();
    if (!worker_ok_) { state_ = UninstallFlowState::failed; message_ = "Game cleanup stopped. Restart Vapor to recover."; return; }
    state_ = UninstallFlowState::finishing;
    if (reinstall_) message_ = "Waiting for Steam to queue the download...";
  }
  if (state_ == UninstallFlowState::finishing) {
    const auto result = operations_.Finish(reinstall_, account_);
    if (result == UninstallFinish::pending) return;
    if (result == UninstallFinish::failed) {
      state_ = UninstallFlowState::failed; message_ = "Game removal could not be finalized. Restart Vapor to recover."; return;
    }
    state_ = UninstallFlowState::complete;
    message_ = reinstall_ ? "Reinstallation queued." : "Game removed; saves and settings were kept.";
  }
}
}
