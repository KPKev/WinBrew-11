#include "../src/steam_integration/vapor_game_cleanup.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace ww::steam_integration;
namespace fs = std::filesystem;
int main() {
  char root[] = "/tmp/vapor-game-cleaner-client-XXXXXX"; assert(mkdtemp(root));
  const fs::path path(root);
  int fd = open((path / "wine11-install.lock").c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600); assert(fd >= 0 && close(fd) == 0);
  auto games = playden::platform::ManagedDirectory::Open(path, playden::platform::DirectoryPolicy::kPrivate); assert(games);
  auto lease = playden::platform::ManagedFileLock::Acquire(*games, "wine11-install.lock", playden::platform::LockMode::kPublisher); assert(lease);
  const vapor_cleaner::Request request{500, "47fc159f-096c-4399-a934-4455637be846", 42, 91, "FAKE78999"};
  const auto text = vapor_cleaner::Encode(request);
  auto put = [&](const char* name, const std::string& contents) { std::ofstream out(path / name); out << contents; out.close(); assert(chmod((path / name).c_str(), 0600) == 0); };
  auto exists = [&](const char* name) { return fs::exists(path / name); };
  bool journal = true; unsigned sends = 0; auto state = ProtectedState::retired;
  auto guard = [&] { return journal; }; auto observe = [&] { return state; }; auto wait = [] {};
  auto successful = [&] { ++sends; state = ProtectedState::removed; put(vapor_cleaner::kResult, text + "ok\n"); return true; };
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 2) == CleanerState::complete);
  assert(sends == 1 && !exists(vapor_cleaner::kRequest) && !exists(vapor_cleaner::kResult));
  state = ProtectedState::retired;
  auto ambiguous = [&] { ++sends; return false; };
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, ambiguous, wait, 2) == CleanerState::pending);
  assert(sends == 2 && exists(vapor_cleaner::kRequest));
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 2) == CleanerState::pending);
  assert(sends == 2); // No automatic resend after an uncertain send.
  state = ProtectedState::removed; put(vapor_cleaner::kResult, text + "ok\n");
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 2) == CleanerState::complete);
  assert(sends == 2 && !exists(vapor_cleaner::kRequest) && !exists(vapor_cleaner::kResult));
  state = ProtectedState::retired; put(vapor_cleaner::kRequest, "not this request\n");
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 1) == CleanerState::blocked);
  assert(sends == 2 && exists(vapor_cleaner::kRequest)); fs::remove(path / vapor_cleaner::kRequest);
  put(vapor_cleaner::kResult, "old result\n");
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 1) == CleanerState::blocked);
  assert(sends == 2); fs::remove(path / vapor_cleaner::kResult);
  journal = false;
  assert(RunGameCleanupHelper(*games, *lease, request, guard, observe, successful, wait, 1) == CleanerState::blocked);
  assert(sends == 2 && !exists(vapor_cleaner::kRequest));
  lease.reset(); games.reset(); fs::remove_all(path);
}
