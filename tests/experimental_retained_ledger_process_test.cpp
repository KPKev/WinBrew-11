// Separate-process host test only; uses an exclusively created temporary fixture.
#include "experimental_retained_ledger.hpp"
#include "../src/steam_integration/native_paths.hpp"
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

using Context = ExperimentalRetainedLedger::Context;
static constexpr Context first{111, 222, 333, 11}, second{112, 222, 333, 22};
static int Child(const char* root, const char* mode) {
  if (!std::strcmp(mode, "contended")) {
    const auto state = playden::platform::ManagedDirectory::Open(
        std::filesystem::path(root) / "wine11-steam", playden::platform::DirectoryPolicy::kPrivate);
    if (!state) return 10;
    auto lock = playden::platform::ManagedFileLock::Acquire(*state, "wine11-state.lock",
        playden::platform::LockMode::kPublisher);
    return !lock && (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : 11;
  }
  int anchor = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (anchor < 0) return 10;
  auto paths = ww::steam_integration::PrepareNativePaths(root, anchor);
  close(anchor);
  if (!paths) return 12;
  ExperimentalRetainedLedger ledger(paths->state, paths->state_lease);
  if (!std::strcmp(mode, "seed"))
    return ledger.Initialize() && ledger.Retain(440, first) && ledger.Retain(730, second) ? 0 : 13;
  auto entries = ledger.Read();
  return entries && entries->size() == 2 && entries->at(440) == first && entries->at(730) == second &&
    !ledger.Retain(440, {111, 222, 999, 11}) ? 0 : 14;
}
static void Run(const char* executable, const char* root, const char* mode) {
  const auto pid = fork(); assert(pid >= 0);
  if (pid == 0) { execl(executable, executable, root, mode, static_cast<char*>(nullptr)); _exit(127); }
  int status = 0; assert(waitpid(pid, &status, 0) == pid);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
int main(int argc, char** argv) {
  if (argc == 3) return Child(argv[1], argv[2]);
  char root[] = "/tmp/wine11-retained-process-XXXXXX"; assert(mkdtemp(root));
  Run(argv[0], root, "seed"); // exits and releases the lease
  Run(argv[0], root, "read"); // separate process reopens durable records
  {
    int anchor = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC); assert(anchor >= 0);
    auto paths = ww::steam_integration::PrepareNativePaths(root, anchor); assert(paths);
    close(anchor);
    Run(argv[0], root, "contended"); // child cannot acquire publisher lease
  }
  Run(argv[0], root, "read"); // lease becomes available after owner exits scope
  std::filesystem::remove_all(root);
}
