"""Generate a pinned Playden service overlay exposing live exit quiescence."""
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
h = replace(h, '  std::optional<GameId> active_;',
            '  bool removal_active_ = false;\n  std::optional<GameId> active_;')
c = replace(c, 'namespace playden::installs {', '''#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
extern "C" { [[gnu::used]] int wine11_cancel_guard_service_enabled = 1; }
#endif
namespace playden::installs {''')
c = replace(c, 'void InstallService::Shutdown() {', '''bool InstallService::WorkerIdleForExit() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || active_ || removal_active_ || !removals_.empty()) return false;
  for (const auto& job : jobs_)
    if (job.view.state == InstallJobState::kQueued || job.view.state == InstallJobState::kRunning) return false;
  return true;
}

void InstallService::Shutdown() {''')
c = replace(c, 'void InstallService::Cancel(GameId id) {', '''void InstallService::Cancel(GameId id) {
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
  (void)id;
  return;
#endif''')
c = replace(c, 'void InstallService::Uninstall(GameId id) {', '''void InstallService::Uninstall(GameId id) {
#if defined(WINE11_DISABLE_UNSAFE_CANCEL)
  (void)id;
  return;
#endif''')
c = replace(c, '        removal = removals_.front();\n        removals_.pop_front();',
            '        removal = removals_.front();\n        removals_.pop_front();\n        removal_active_ = true;')
c = replace(c, '      std::lock_guard<std::mutex> lock(mutex_);\n      if (const auto& game = removal->game) {',
            '      std::lock_guard<std::mutex> lock(mutex_);\n      removal_active_ = false;\n      if (const auto& game = removal->game) {')
c = replace(c, '        remove_files = true;', '        removal_active_ = true;\n        remove_files = true;')
c = replace(c, '    if (remove_files) runner->Remove(next_root);', '''    if (remove_files) {
      runner->Remove(next_root);
      std::lock_guard<std::mutex> lock(mutex_);
      removal_active_ = false;
    }''')
(destination / 'installs').mkdir(parents=True, exist_ok=True)
(destination / 'installs/install_service.h').write_text(h, encoding='utf-8', newline='\n')
(destination / 'installs/install_service.cpp').write_text(c, encoding='utf-8', newline='\n')
