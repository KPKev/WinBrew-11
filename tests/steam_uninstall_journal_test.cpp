#include "../src/steam_integration/uninstall_journal.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
using namespace ww::steam_integration;
static std::atomic<bool> fail_after_rename{false}, fail_directory_sync{false};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int from, const char* oldname, int to, const char* newname) {
  const int result = __real_renameat(from, oldname, to, newname);
  if (result == 0 && fail_after_rename.exchange(false)) fail_directory_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && fail_directory_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/vapor-uninstall-journal-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = PrepareNativePaths(root, anchor); assert(paths);
    auto lease = playden::platform::ManagedFileLock::Acquire(paths->games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
    UninstallJournal journal(paths->state, paths->state_lease, paths->games, *lease);
    using Phase = UninstallJournal::Phase;
    assert(journal.Read().phase == Phase::absent);
    assert(mkdir((paths->games.path() / "365670").c_str(), 0700) == 0);
    const std::string registry = "489830\tinternal\t123\n365670\tinternal\t1000\n";
    { std::ofstream out(paths->games.path() / ".playden-games"); out << registry; }
    assert(chmod((paths->games.path() / ".playden-games").c_str(), 0600) == 0);
    struct stat st{}; assert(paths->games.InspectChild("365670", &st));
    UninstallJournal::Entry entry{Phase::intent, 365670, "11111111-2222-4333-8444-555555555555", static_cast<std::uint64_t>(st.st_dev),
      static_cast<std::uint64_t>(st.st_ino), "FAKE78008", true};
    entry.account = 101;
    entry.registry_before = registry;
    auto wrong = entry; ++wrong.root_inode;
    assert(!journal.Begin(wrong)); assert(journal.Read().phase == Phase::absent);
    // A rename can succeed even when the following directory sync fails.
    // The caller must stop; a later durable read rediscovers the intent.
    fail_after_rename = true;
    assert(!journal.Begin(entry));
    assert(journal.Read().phase == Phase::intent);
    assert(!journal.Begin(entry));
    assert(!journal.Advance(entry, Phase::game_retired));
    assert(!journal.Clear(entry));
    fail_after_rename = true;
    assert(!journal.Advance(entry, Phase::title_removed));
    // The published record may already have advanced; the old state cannot
    // trigger a second title removal before a durable re-read.
    assert(journal.Read().phase == Phase::title_removed);
    assert(!journal.Advance(entry, Phase::title_removed));
    // A fresh journal instance observes the same operation after a restart.
    UninstallJournal resumed(paths->state, paths->state_lease, paths->games, *lease);
    entry = resumed.Read(); assert(entry.phase == Phase::title_removed && entry.app == 365670 && entry.reinstall);
    assert(resumed.Advance(entry, Phase::game_retired));
    entry = resumed.Read(); assert(resumed.Advance(entry, Phase::registry_removed));
    entry = resumed.Read(); assert(!resumed.Clear(entry));
    assert(resumed.Advance(entry, Phase::quarantine_removed));
    entry = resumed.Read(); assert(!resumed.Clear(entry));
    assert(resumed.Advance(entry, Phase::queue_intent));
    entry = resumed.Read(); assert(!resumed.Clear(entry));
    assert(resumed.Advance(entry, Phase::download_queued));
    entry = resumed.Read(); assert(resumed.Clear(entry)); assert(resumed.Read().phase == Phase::absent);
    // Unknown or damaged state is never interpreted as permission to start.
    { std::ofstream damaged(paths->state.path() / "vapor-uninstall.v1"); damaged << "broken\n"; }
    assert(resumed.Read().phase == Phase::blocked);
    assert(!resumed.Begin(UninstallJournal::Entry{Phase::intent, 365670, "11111111-2222-4333-8444-555555555556",
      static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino), "FAKE78008", true}));
  }
  close(anchor); std::filesystem::remove_all(root);
}

