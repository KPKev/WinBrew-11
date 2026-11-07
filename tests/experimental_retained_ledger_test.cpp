#include "experimental_retained_ledger.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

static std::atomic<bool> arm{false}, fail_sync{false}, fail_file_sync{false};
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
  const auto result = __real_renameat(a, b, c, d);
  if (!result && arm.exchange(false)) fail_sync = true;
  return result;
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st{};
  if (!fstat(fd, &st) && S_ISREG(st.st_mode) && fail_file_sync.exchange(false)) { errno = EIO; return -1; }
  if (!fstat(fd, &st) && S_ISDIR(st.st_mode) && fail_sync.exchange(false)) { errno = EIO; return -1; }
  return __real_fsync(fd);
}
int main() {
  char root[] = "/tmp/wine11-retained-ledger-XXXXXX"; assert(mkdtemp(root));
  int anchor = open(root, O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    ExperimentalRetainedLedger first(paths->state, paths->state_lease);
    using Context = ExperimentalRetainedLedger::Context;
    const Context a{111, 222, 333, 11}, b{111, 222, 333, 22}, c{111, 222, 333, 33};
    assert(!first.Read() && !first.Retain(1, a));
    assert(first.Initialize() && !first.Initialize());
    assert(first.Retain(440, a) && first.Retain(730, b));
    assert(!first.Retain(440, {111, 222, 333, 99})); // newer attempt
    assert(!first.Retain(440, {112, 222, 333, 11})); // another account
    assert(!first.Retain(440, {111, 223, 333, 11})); // another volume
    assert(!first.Retain(440, {111, 222, 334, 11})); // replacement volume
    assert(!first.Retain(200, {111, 222, 0, 44}));
    ExperimentalRetainedLedger restart(paths->state, paths->state_lease);
    auto entries = restart.Read(); assert(entries && entries->size() == 2 && entries->at(440) == a && entries->at(730) == b);
    // Rename is visible before the injected directory-sync failure: no acknowledgement.
    arm = true; assert(!restart.Retain(570, c));
    assert(!arm && !fail_sync);
    entries = first.Read(); assert(entries && entries->size() == 3 && entries->at(570) == c);
    assert(first.Retain(570, c)); // durable reread resolves the indeterminate result
    fail_file_sync = true; assert(!restart.Retain(100, {111, 222, 333, 44}));
    entries = first.Read(); assert(entries && entries->size() == 3);
    std::thread t1([&] { assert(first.Retain(100, {111, 222, 333, 44})); });
    std::thread t2([&] { assert(restart.Retain(200, {111, 222, 333, 55})); });
    t1.join(); t2.join();
    entries = first.Read(); assert(entries && entries->size() == 5 && entries->at(100).attempt == 44 && entries->at(200).attempt == 55);
    playden::Storage storage(paths->state.path().string());
    for (const std::string invalid : {"retained 2\n440 111 222 333 11\n440 111 222 333 12\n",
          "retained 1\n440 11\n", "retained 2\n440 111 222 0 11\n",
          "retained 2\n4294967296 111 222 333 11\n", "retained 2\n440 111 222 333 11"}) {
      assert(storage.WriteDurable("host-retained-downloads.v1", {invalid.begin(), invalid.end()}).ok());
      assert(!restart.Read() && !restart.Retain(10, a));
    }
  }
  close(anchor); std::filesystem::remove_all(root);
}
