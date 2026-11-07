#include "../src/steam_integration/uninstall_journal.hpp"
#include "../src/steam_integration/uninstall_registry.hpp"
#include "../src/steam_integration/protected_uninstall.hpp"
#include "installs/install_service.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace ww::steam_integration;
using namespace std::chrono_literals;
static std::atomic<bool> arm_rename{false}, arm_unlink{false}, fail_sync{false};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
  int result = __real_renameat(a, b, c, d); if (result == 0 && arm_rename.exchange(false)) fail_sync = true; return result;
}
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* path, int flags) {
  int result = __real_unlinkat(fd, path, flags); if (result == 0 && arm_unlink.exchange(false)) fail_sync = true; return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{}; if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
struct Runner : playden::installs::InstallRunner {
  std::atomic<unsigned> calls{0}, last_app{0};
  Result Install(const playden::GameId& id, const std::string&, const std::atomic<bool>& cancel,
      const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
      const std::function<void(playden::installs::InstallStep)>&) override {
    last_app = id.steam_app().value_or(0); ++calls; while (!cancel.load()) std::this_thread::sleep_for(1ms);
    return {false, true, "", 0, false, ""};
  }
  std::string Remove(const std::string&) override { assert(false); return {}; }
};
int main() {
  assert(geteuid() == 0);
  const char* user = std::getenv("SUDO_UID"); const uid_t app_uid = user ? static_cast<uid_t>(std::strtoul(user, nullptr, 10)) : 1000;
  assert(app_uid != 0);
  char name[] = "/tmp/vapor-recovery-XXXXXX"; assert(mkdtemp(name)); assert(chmod(name, 0755) == 0);
  const std::filesystem::path base(name), state_path = base / "wine11-steam", games_path = base / "wine11-games";
  for (const auto& p : {state_path, games_path}) assert(mkdir(p.c_str(), 0700) == 0 && chown(p.c_str(), app_uid, -1) == 0);
  { std::ofstream outside(base / "outside"); outside << "keep"; }
  assert(seteuid(app_uid) == 0);
  for (const auto& p : {state_path / "wine11-state.lock", games_path / "wine11-install.lock"}) {
    int fd = open(p.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600); assert(fd >= 0 && close(fd) == 0);
  }
  auto state = playden::platform::ManagedDirectory::Open(state_path, playden::platform::DirectoryPolicy::kPrivate); assert(state);
  auto games = playden::platform::ManagedDirectory::Open(games_path, playden::platform::DirectoryPolicy::kPrivate); assert(games);
  auto state_lock = playden::platform::ManagedFileLock::Acquire(*state, "wine11-state.lock", playden::platform::LockMode::kPublisher); assert(state_lock);
  auto games_lock = playden::platform::ManagedFileLock::Acquire(*games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(games_lock);
  const auto game = games_path / "365670"; assert(mkdir(game.c_str(), 0700) == 0);
  { std::ofstream out(game / "game.bin"); out << "old game"; }
  const std::string registry = "489830\tinternal\t123\n365670\tinternal\t1000\n730\tinternal\t0\tpaused\n";
  { std::ofstream out(games_path / ".playden-games"); out << registry; } assert(chmod((games_path / ".playden-games").c_str(), 0666) == 0);
  struct stat st{}; assert(games->InspectChild("365670", &st));
  using P = UninstallJournal::Phase;
  UninstallJournal::Entry e{P::intent, 365670, "11111111-2222-4333-8444-555555555555",
      static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino), "FAKE78008", true};
  e.account = 101; e.registry_before = registry;
  UninstallJournal journal(*state, *state_lock, *games, *games_lock); assert(journal.Begin(e));
  // The title removal itself is simulated; production requires independently confirmed PS5 absence.
  assert(journal.Advance(e, P::title_removed)); e = journal.Read();
  const UninstallTarget target{e.app, e.operation, e.root_device, e.root_inode};
  assert(RetireProtectedGame(*games, *games_lock, base, target) == ProtectedState::retired);
  // A fresh journal after a crash sees the retired folder and advances once.
  UninstallJournal recovered(*state, *state_lock, *games, *games_lock);
  assert(InspectProtectedGame(*games, *games_lock, base, target) == ProtectedState::retired);
  assert(recovered.Advance(e, P::game_retired)); e = recovered.Read();
  const auto plan = PlanInstalledRegistryRow(e.app, e.registry_before); assert(plan);
  arm_rename = true; assert(RemoveInstalledRegistryRow(*games, *games_lock, *plan) == RegistryRemoval::indeterminate);
  assert(RemoveInstalledRegistryRow(*games, *games_lock, *plan) == RegistryRemoval::already_absent);
  assert(recovered.Advance(e, P::registry_removed)); e = recovered.Read();
  arm_unlink = true; assert(RemoveProtectedGame(*games, *games_lock, base, target) == ProtectedState::blocked);
  assert(InspectProtectedGame(*games, *games_lock, base, target) == ProtectedState::retired);
  assert(RemoveProtectedGame(*games, *games_lock, base, target) == ProtectedState::removed);
  assert(recovered.Advance(e, P::quarantine_removed)); e = recovered.Read();
  assert(recovered.Advance(e, P::queue_intent)); e = recovered.Read();
  arm_rename = true; assert(QueueReinstallRegistry(*games, *games_lock, *plan) == QueueReinstall::indeterminate);
  assert(QueueReinstallRegistry(*games, *games_lock, *plan) == QueueReinstall::already_queued);
  assert(recovered.Advance(e, P::download_queued)); e = recovered.Read(); assert(recovered.Clear(e));
  assert(InspectReinstallRegistry(*games, *games_lock, *plan) == ReinstallRegistry::queued);
  {
    Runner runner; playden::installs::InstallService service(runner, games_path.string());
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (!runner.calls.load() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    assert(runner.calls == 1 && runner.last_app == 365670);
    const auto snapshot = service.Snapshot();
    bool counter_strike_paused = false;
    for (const auto& job : snapshot.jobs)
      if (job.id == playden::GameId::Steam(730) && job.state == playden::installs::InstallJobState::kPaused)
        counter_strike_paused = true;
    assert(counter_strike_paused);
  }
  assert(std::filesystem::exists(base / "outside"));
  games_lock.reset(); state_lock.reset(); games.reset(); state.reset();
  assert(seteuid(0) == 0); std::filesystem::remove_all(base);
}
