"""Generate an in-process cancellation cleanup overlay for the pinned service.

This does not persist cleanup intent across a crash or restart.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
destination = Path(sys.argv[1])
source = root / 'vendor/playden-source/src/installs'
h = (source / 'install_service.h').read_text()
c = (source / 'install_service.cpp').read_text()

def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Pinned install service changed; refusing cleanup overlay')
    return text.replace(old, new)

h = replace(h, '  std::vector<GameId> uninstalling;  // games whose files are being deleted\n',
            '  std::vector<GameId> uninstalling;  // games whose files are being deleted\n'
            '  std::vector<GameId> cancel_cleanup;\n'
            '  std::vector<RemovalFailure> cancel_cleanup_failures;\n'
            '  std::vector<GameId> cancel_cleanup_retryable;\n')
h = replace(h, '  std::uint64_t revision = 0;\n  std::vector<InstallJobView> jobs;',
            '  std::uint64_t revision = 0;\n  bool recovery_blocked = false;\n  std::vector<InstallJobView> jobs;')
h = replace(h, '  InstallService(Runners runners, std::vector<InstallVolume> volumes);',
            '  InstallService(Runners runners, std::vector<InstallVolume> volumes);\n'
            '  InstallService(Runners runners, std::vector<InstallVolume> volumes, std::function<bool()> before_scan);\n'
            '  InstallService(Runners runners, std::vector<InstallVolume> volumes, std::function<bool()> before_scan, std::function<bool(GameId)> before_cleanup);')
h = replace(h, '  Runners runners_;', '  Runners runners_;\n  std::function<bool(GameId)> before_cleanup_;\n  std::deque<GameId> pending_preparations_;')
h = replace(h, '  bool stopping_ = false;\n  std::thread worker_;',
            '  bool stopping_ = false;\n  bool recovery_blocked_ = false;\n  std::thread worker_;')
h = replace(h, '    std::optional<InstalledGame> game;  // an uninstall\'s folder; none for a canceled download\n',
            '    std::optional<InstalledGame> game;  // an uninstall\'s folder; none for a canceled download\n'
            '    std::optional<GameId> canceled_job;\n')
h = replace(h, '  std::vector<RemovalFailure> removal_failures_;\n',
            '  std::vector<RemovalFailure> removal_failures_;\n'
            '  std::vector<GameId> cancel_cleanup_;\n'
            '  std::vector<RemovalFailure> cancel_cleanup_failures_;\n'
            '  std::vector<std::pair<GameId, std::string>> cancel_cleanup_roots_;\n')
c = replace(c, '''InstallService::InstallService(Runners runners, std::vector<InstallVolume> volumes)
    : runners_(std::move(runners)), volumes_(std::move(volumes)) {''',
'''InstallService::InstallService(Runners runners, std::vector<InstallVolume> volumes)
    : InstallService(std::move(runners), std::move(volumes), {}) {}

InstallService::InstallService(Runners runners, std::vector<InstallVolume> volumes, std::function<bool()> before_scan)
    : InstallService(std::move(runners), std::move(volumes), std::move(before_scan), {}) {}

InstallService::InstallService(Runners runners, std::vector<InstallVolume> volumes, std::function<bool()> before_scan, std::function<bool(GameId)> before_cleanup)
    : runners_(std::move(runners)), before_cleanup_(std::move(before_cleanup)), volumes_(std::move(volumes)) {''')
c = replace(c, '''  default_volume_id_ = volumes_.front().id;
  Scan();''', '''  default_volume_id_ = volumes_.front().id;
  bool admitted = true;
  if (before_scan) {
#if defined(__cpp_exceptions)
    try { admitted = before_scan(); } catch (...) { admitted = false; }
#else
    admitted = before_scan();
#endif
  }
  if (!admitted) { recovery_blocked_ = true; stopping_ = true; return; }
  Scan();''')
c = replace(c, '  snapshot.revision = revision_;',
            '  snapshot.revision = revision_;\n  snapshot.recovery_blocked = recovery_blocked_;')
c = replace(c, '''  auto* job = Find(id);
  if (!job) return;
  job->cancel_requested = true;''', '''  auto* job = Find(id);
  if (!job) {
    const auto failed = std::find_if(cancel_cleanup_failures_.begin(), cancel_cleanup_failures_.end(),
        [id](const RemovalFailure& entry) { return entry.id == id; });
    const auto root = std::find_if(cancel_cleanup_roots_.begin(), cancel_cleanup_roots_.end(),
        [id](const auto& entry) { return entry.first == id; });
    if (failed != cancel_cleanup_failures_.end() && root != cancel_cleanup_roots_.end() && !root->second.empty()) {
      cancel_cleanup_.push_back(id);
      QueueRemoval({id.store, root->second, std::nullopt, std::nullopt, id});
      cancel_cleanup_failures_.erase(failed);
      cancel_cleanup_roots_.erase(root);
      Publish();
      wake_.notify_all();
    }
    return;
  }
  if (job->cancel_requested) return;
  job->cancel_requested = true;''')
c = replace(c, '''  if (active_ == id) {
    active_cancel_.store(true);''', '''  if (active_ == id) {
    cancel_cleanup_.push_back(id);
    active_cancel_.store(true);''')
c = replace(c, '''    const auto* volume = FindVolume(job->view.volume_id);
    const auto root = volume && volume->available ? RootOn(job->view.volume_id, id) : std::string();
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& entry) { return entry.view.id == id; }),
                jobs_.end());''', '''#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
    job->view.state = InstallJobState::kPaused;
    job->view.failure = "Cancellation is awaiting durable cleanup.";
    cancel_cleanup_.push_back(id);
    pending_preparations_.push_back(id);
#else
    const auto* volume = FindVolume(job->view.volume_id);
    const auto root = volume && volume->available ? RootOn(job->view.volume_id, id) : std::string();
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& entry) { return entry.view.id == id; }),
                jobs_.end());''')
c = replace(c, '    if (!root.empty()) QueueRemoval({id.store, root, std::nullopt, std::nullopt});',
'''    if (!root.empty()) {
      cancel_cleanup_.push_back(id);
      QueueRemoval({id.store, root, std::nullopt, std::nullopt, id});
    } else {
      cancel_cleanup_failures_.push_back({id, "Storage unavailable. Reconnect it; cleanup is incomplete."});
    }
#endif''')
c = replace(c, '  // Only a store with a runner installs.\n',
'''  // Do not overwrite files while cancellation cleanup is outstanding or failed.
  if (std::find(cancel_cleanup_.begin(), cancel_cleanup_.end(), id) != cancel_cleanup_.end() ||
      std::any_of(cancel_cleanup_failures_.begin(), cancel_cleanup_failures_.end(),
                  [id](const RemovalFailure& failure) { return failure.id == id; })) return;
  // Only a store with a runner installs.
''')
c = replace(c, '''void InstallService::Resume(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job || job->view.state == InstallJobState::kRunning) return;''',
'''void InstallService::Resume(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job || job->view.state == InstallJobState::kRunning) return;
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
  if (job->cancel_requested) return;
#endif''')
c = replace(c, '''void InstallService::Pause(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job) return;''', '''void InstallService::Pause(GameId id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto* job = Find(id);
  if (!job) return;
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
  if (job->cancel_requested) return;
#endif''')
c = replace(c, '  for (const auto& game : uninstalling_) snapshot.uninstalling.push_back(game.id);',
'''  for (const auto& game : uninstalling_) snapshot.uninstalling.push_back(game.id);
  snapshot.cancel_cleanup = cancel_cleanup_;
  snapshot.cancel_cleanup_failures = cancel_cleanup_failures_;
  for (const auto& entry : cancel_cleanup_roots_) if (!entry.second.empty()) snapshot.cancel_cleanup_retryable.push_back(entry.first);''')
c = replace(c, '      } else if (removal->game) {\n        failure = "Playden can\'t delete this game\'s files.";\n      }',
'''      } else if (removal->game || removal->canceled_job) {
        failure = "Playden can't delete this game's files.";
      }''')
c = replace(c, '        else failure = remover->Remove(removal->root);',
'''        else {
#if defined(__cpp_exceptions)
          try { failure = remover->Remove(removal->root); }
          catch (...) { failure = "Cleanup stopped unexpectedly."; }
#else
          failure = remover->Remove(removal->root);
#endif
        }''')
c = replace(c, '      if (const auto& game = removal->game) {',
'''      if (removal->canceled_job) {
        const auto id = *removal->canceled_job;
        cancel_cleanup_.erase(std::remove(cancel_cleanup_.begin(), cancel_cleanup_.end(), id), cancel_cleanup_.end());
        if (!failure.empty()) {
          cancel_cleanup_failures_.push_back({id, failure});
          cancel_cleanup_roots_.push_back({id, removal->root});
        }
      }
      if (const auto& game = removal->game) {''')
c = replace(c, '''      if (job && result.ok) {
        // Installed''', '''      if (job && result.ok) {
        cancel_cleanup_.erase(std::remove(cancel_cleanup_.begin(), cancel_cleanup_.end(), *next), cancel_cleanup_.end());
        // Installed''')
c = replace(c, '''      } else if (job && job->cancel_requested) {
        jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [&](const Job& entry) { return entry.view.id == *next; }),
                    jobs_.end());
        remove_files = true;''', '''      } else if (job && job->cancel_requested) {
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
        job->view.state = InstallJobState::kPaused;
        job->view.failure = "Cancellation is awaiting durable cleanup.";
        pending_preparations_.push_back(*next);
#else
        jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [&](const Job& entry) { return entry.view.id == *next; }),
                    jobs_.end());
        remove_files = true;
#endif''')
c = replace(c, '''        if (auto* job = Find(*next); job && job->view.state == InstallJobState::kRunning) {
          ClearWait(*job);
          job->view.state = InstallJobState::kFailed;
          job->view.failure = unwritable;
        }''', '''        if (auto* job = Find(*next); job && job->cancel_requested) {
          const auto id = *next;
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
          job->view.state = InstallJobState::kPaused;
          job->view.failure = "Cancellation is awaiting durable cleanup.";
          pending_preparations_.push_back(id);
#else
          jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [id](const Job& entry) { return entry.view.id == id; }), jobs_.end());
          QueueRemoval({id.store, next_root, std::nullopt, std::nullopt, id});
#endif
        } else if (job && job->view.state == InstallJobState::kRunning) {
          ClearWait(*job);
          job->view.state = InstallJobState::kFailed;
          job->view.failure = unwritable;
        }''')
c = replace(c, '    if (remove_files) runner->Remove(next_root);',
'''    if (remove_files) {
      std::string failure;
#if defined(__cpp_exceptions)
      try { failure = runner->Remove(next_root); }
      catch (...) { failure = "Cleanup stopped unexpectedly."; }
#else
      failure = runner->Remove(next_root);
#endif
      std::lock_guard<std::mutex> lock(mutex_);
      cancel_cleanup_.erase(std::remove(cancel_cleanup_.begin(), cancel_cleanup_.end(), *next), cancel_cleanup_.end());
      if (!failure.empty()) {
        cancel_cleanup_failures_.push_back({*next, failure});
        cancel_cleanup_roots_.push_back({*next, next_root});
      }
      Publish();
    }''')
c = replace(c, '    std::optional<Removal> removal;\n    {',
'''    std::optional<Removal> removal;
#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
    std::optional<GameId> prepare;
#endif
    {''')
c = replace(c, '        if (!removals_.empty()) break;',
'''#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
        if (!pending_preparations_.empty()) break;
#endif
        if (!removals_.empty()) break;''')
c = replace(c, '      if (!removals_.empty()) {\n        removal = removals_.front();',
'''#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
      if (!pending_preparations_.empty()) {
        prepare = pending_preparations_.front();
        pending_preparations_.pop_front();
      } else
#endif
      if (!removals_.empty()) {
        removal = removals_.front();''')
c = replace(c, '    if (removal) {\n      std::string failure;',
'''#if defined(WINE11_DURABLE_CANCEL_QUIESCE)
    if (prepare) {
      bool saved = false;
#if defined(__cpp_exceptions)
      try { saved = before_cleanup_ && before_cleanup_(*prepare); } catch (...) { saved = false; }
#else
      saved = before_cleanup_ && before_cleanup_(*prepare);
#endif
      std::lock_guard<std::mutex> lock(mutex_);
      if (auto* job = Find(*prepare); job && job->cancel_requested) {
        job->view.failure = saved ? "Cancellation saved; cleanup remains pending."
                                  : "Cancellation could not be saved; files were kept.";
      }
      Publish();
      continue;
    }
#endif
    if (removal) {
      std::string failure;''')
(destination / 'installs').mkdir(parents=True, exist_ok=True)
(destination / 'installs/install_service.h').write_text(h)
(destination / 'installs/install_service.cpp').write_text(c)
