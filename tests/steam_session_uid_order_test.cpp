// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam/local_session_repository.h"
#include "platform/ps5/title_allow_list.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unistd.h>
int main() {
  char path[] = "/tmp/wine11-session-order-XXXXXX"; assert(mkdtemp(path));
  playden::SessionVault vault(false);
  playden::LocalSessionRepository repo(path, {}, &vault);
  std::atomic<bool> started{false}, finished{false};
  auto held = playden::ps5::HoldEffectiveUid();
  std::thread worker([&] { started = true; assert(repo.Load().ok()); finished = true; });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!started && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(started && !finished);
  held.reset(); worker.join(); assert(finished);
  std::filesystem::remove_all(path);
}
