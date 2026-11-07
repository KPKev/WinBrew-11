"""Generate an isolated, unapproved cleanup-failure experiment."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit('usage: prepare_steam_quiescence.py OUTPUT_ROOT')
destination = Path(sys.argv[1])
h = (root / 'vendor/playden-source/src/installs/install_service.h').read_text()
c = (root / 'vendor/playden-source/src/installs/install_service.cpp').read_text()

def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Pinned install service changed; refusing ambiguous quiescence patch')
    return text.replace(old, new)

h = replace(h, '  InstallSnapshot Snapshot();',
            '  InstallSnapshot Snapshot();\n  // Live, synchronized worker/queue state; not a cleanup-success guarantee.\n  bool WorkerIdleForExit();')
h = replace(h, '  std::string failure;  // short, user-facing\n',
            '  std::string failure;  // short, user-facing\n  bool cleanup_failed = false, cleanup_retryable = false;\n')
h = replace(h, '  std::optional<GameId> active_;',
            '  bool removal_active_ = false;\n  std::optional<GameId> active_;')
h = replace(h, "    std::optional<InstalledGame> game;  // an uninstall's folder; none for a canceled download\n",
            "    std::optional<InstalledGame> game;  // an uninstall's folder; none for a canceled download\n"
            '    std::optional<GameId> canceled_job;\n')
h = replace(h, '  void QueueRemoval(Removal removal);',
            '  void QueueRemoval(Removal removal);\n  void FinishCancelCleanup(GameId id, const std::string& failure);')
c = replace(c, '#include <charconv>', '#include <charconv>\n#include <cerrno>')
c = replace(c, '  bool removing = false;  // "<id>\\t<volume>\\t<install bytes>\\tremoving": uninstalled, folder not deleted yet',
            '  bool removing = false;  // "<id>\\t<volume>\\t<install bytes>\\tremoving": uninstalled, folder not deleted yet\n  bool cleanup_pending = false;')
c = replace(c, '    entry.removing = flag == "removing";',
            '    entry.removing = flag == "removing";\n    entry.cleanup_pending = flag == "cleanup-pending";')
c = replace(c, '''    if (!volume || !volume->available) {
      // Keep games on an absent drive listed so they come back when it does.''', '''    if (entry.cleanup_pending) {
      Job job; job.view.id = entry.id; job.view.volume_id = entry.volume_id;
      job.view.state = InstallJobState::kFailed; job.view.cleanup_failed = true;
      job.view.cleanup_retryable = volume && volume->available;
      job.view.failure = "Cleanup was interrupted. Select Retry cleanup.";
      if (job.view.cleanup_retryable) {
        const auto record = RootOn(entry.volume_id, entry.id) + "/" + kRecord;
        struct stat info{};
        if (lstat(record.c_str(), &info) == 0 || errno != ENOENT) {
          job.view.cleanup_retryable = false;
          job.view.failure = "Cleanup needs attention: an install record exists or cannot be inspected.";
        }
      } else job.view.failure = "Storage unavailable. Reconnect it; cleanup needs attention.";
      if (jobs_.size() < kMaxJobs) jobs_.push_back(std::move(job));
      continue;
    }
    if (!volume || !volume->available) {
      // Keep games on an absent drive listed so they come back when it does.''')
c = replace(c, '''    const bool waits = job.view.state == InstallJobState::kPaused || job.view.state == InstallJobState::kFailed;
    std::fprintf(registry, "%s\\t%s\\t0%s\\n", ToKey(job.view.id).c_str(), job.view.volume_id.c_str(), waits ? "\\tpaused" : "");''',
'''    const bool waits = job.view.state == InstallJobState::kPaused || job.view.state == InstallJobState::kFailed;
    const char* flag = job.cancel_requested || job.view.cleanup_failed ? "\\tcleanup-pending" : waits ? "\\tpaused" : "";
    std::fprintf(registry, "%s\\t%s\\t0%s\\n", ToKey(job.view.id).c_str(), job.view.volume_id.c_str(), flag);''')
c = replace(c, '''void InstallService::Cancel(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job) return;
  job->cancel_requested = true;''', '''void InstallService::Cancel(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job || job->cancel_requested || (job->view.cleanup_failed && !job->view.cleanup_retryable)) return;
  job->view.cleanup_failed = false;
  job->view.cleanup_retryable = false;
  job->cancel_requested = true;''')
c = replace(c, '''    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& entry) { return entry.view.id == id; }),
                jobs_.end());
    if (!root.empty()) QueueRemoval({id.store, root, std::nullopt, std::nullopt});''', '''    if (!root.empty()) {
      job->view.state = InstallJobState::kPaused;
      job->view.failure = "Removing partial download...";
      QueueRemoval({id.store, root, std::nullopt, std::nullopt, id});
    } else {
      job->cancel_requested = false;
      job->view.state = InstallJobState::kFailed;
      job->view.failure = "Storage unavailable. Reconnect it, then select Cancel to retry cleanup.";
      job->view.cleanup_failed = true;
      job->view.cleanup_retryable = false;
    }''')
c = replace(c, '''void InstallService::Pause(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job) return;''', '''void InstallService::Pause(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job || job->cancel_requested || job->view.cleanup_failed) return;''')
c = replace(c, '  if (!job || job->view.state == InstallJobState::kRunning) return;',
            '  if (!job || job->cancel_requested || job->view.cleanup_failed || job->view.state == InstallJobState::kRunning) return;')
c = replace(c, 'void InstallService::QueueRemoval(Removal removal) {', '''void InstallService::FinishCancelCleanup(GameId id, const std::string& failure) {
  auto* job = Find(id);
  if (!job || !job->cancel_requested) return;
  if (failure.empty()) {
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& entry) { return entry.view.id == id; }), jobs_.end());
  } else {
    job->cancel_requested = false;
    job->view.state = InstallJobState::kFailed;
    job->view.failure = "Cleanup failed. Select Cancel to retry. " + failure;
    job->view.cleanup_failed = true;
    job->view.cleanup_retryable = true;
  }
}

void InstallService::QueueRemoval(Removal removal) {''')
c = replace(c, '''      if (auto* remover = RunnerFor(removal->store)) {
        if (removal->forget) remover->Forget(*removal->forget);
        else failure = remover->Remove(removal->root);
      } else if (removal->game) {''', '''      if (auto* remover = RunnerFor(removal->store)) {
#if defined(__cpp_exceptions)
        try {
#endif
          if (removal->forget) remover->Forget(*removal->forget);
          else failure = remover->Remove(removal->root);
#if defined(__cpp_exceptions)
        } catch (...) { failure = "Cleanup stopped unexpectedly."; }
#endif
      } else if (removal->game || removal->canceled_job) {''')
c = replace(c, '      if (const auto& game = removal->game) {',
            '      if (removal->canceled_job) FinishCancelCleanup(*removal->canceled_job, failure);\n      if (const auto& game = removal->game) {')
c = replace(c, '''      } else if (job && job->cancel_requested) {
        jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [&](const Job& entry) { return entry.view.id == *next; }),
                    jobs_.end());
        remove_files = true;''', '''      } else if (job && job->cancel_requested) {
        job->view.state = InstallJobState::kPaused;
        job->view.failure = "Removing partial download...";
        remove_files = true;''')
c = replace(c, '''        if (auto* job = Find(*next); job && job->view.state == InstallJobState::kRunning) {
          ClearWait(*job);
          job->view.state = InstallJobState::kFailed;
          job->view.failure = unwritable;
        }''', '''        if (auto* job = Find(*next); job && job->cancel_requested) {
          job->view.state = InstallJobState::kPaused;
          job->view.failure = "Removing partial download...";
          QueueRemoval({next->store, next_root, std::nullopt, std::nullopt, *next});
        } else if (job && job->view.state == InstallJobState::kRunning) {
          ClearWait(*job);
          job->view.state = InstallJobState::kFailed;
          job->view.failure = unwritable;
        }''')
c = replace(c, 'void InstallService::Shutdown() {', '''bool InstallService::WorkerIdleForExit() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || active_ || removal_active_ || !removals_.empty()) return false;
  for (const auto& job : jobs_)
    if (job.view.state == InstallJobState::kQueued || job.view.state == InstallJobState::kRunning) return false;
  return true;
}

void InstallService::Shutdown() {''')
c = replace(c, '        removal = removals_.front();\n        removals_.pop_front();',
            '        removal = removals_.front();\n        removals_.pop_front();\n        removal_active_ = true;')
c = replace(c, '      std::lock_guard<std::mutex> lock(mutex_);\n      if (removal->canceled_job) FinishCancelCleanup(*removal->canceled_job, failure);',
            '      std::lock_guard<std::mutex> lock(mutex_);\n      removal_active_ = false;\n      if (removal->canceled_job) FinishCancelCleanup(*removal->canceled_job, failure);')
c = replace(c, '        remove_files = true;', '        removal_active_ = true;\n        remove_files = true;')
c = replace(c, '    if (remove_files) runner->Remove(next_root);', '''    if (remove_files) {
      std::string failure;
#if defined(__cpp_exceptions)
      try { failure = runner->Remove(next_root); }
      catch (...) { failure = "Cleanup stopped unexpectedly."; }
#else
      failure = runner->Remove(next_root);
#endif
      std::lock_guard<std::mutex> lock(mutex_);
      removal_active_ = false;
      FinishCancelCleanup(*next, failure);
      Publish();
    }''')
(destination / 'installs').mkdir(parents=True, exist_ok=True)
(destination / 'installs/install_service.h').write_text(h, encoding='utf-8', newline='\n')
(destination / 'installs/install_service.cpp').write_text(c, encoding='utf-8', newline='\n')
