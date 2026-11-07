// SPDX-License-Identifier: GPL-3.0-or-later
#include "steam/cloud_service.h"
#include "platform/ps5/title_allow_list.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unistd.h>
using namespace playden;
struct Sessions : SessionRepository {
  Result<std::optional<StoredSession>> Load() const override { return Result<std::optional<StoredSession>>::Success(StoredSession{42,"Player","r","a",{}}); }
  Result<void> Save(const StoredSession&) const override { return Result<void>::Success(); }
  Result<void> Clear() const override { return Result<void>::Success(); }
};
struct Runner : steam::CloudRunner {
  std::atomic<bool> entered{false}, release{false};
  steam::CloudReport Check(const StoredSession&, const steam::CloudLocation&, const std::atomic<bool>& cancel) override {
    entered = true; while (!release && !cancel) std::this_thread::yield();
    steam::CloudReport report; report.state = steam::CloudState::kUpToDate; return report;
  }
  steam::CloudReport Apply(const StoredSession&, const steam::CloudLocation&, steam::CloudResolution, bool,
                           const std::atomic<bool>&) override { return {}; }
};
int main() {
  char path[] = "/tmp/wine11-cloud-uid-XXXXXX"; assert(mkdtemp(path));
  Sessions sessions; Runner runner; steam::CloudService service(runner, sessions);
  steam::CloudLocation location{GameId::Steam(440), path, path, path};
  assert(service.Sync(location));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!runner.entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(runner.entered && !ps5::TryHoldEffectiveUid());
  runner.release = true;
  bool finished = false;
  while (!finished && std::chrono::steady_clock::now() < deadline) {
    for (const auto& game : service.Snapshot().games)
      if (game.id == location.id && game.report.state == steam::CloudState::kUpToDate) finished = true;
    std::this_thread::yield();
  }
  assert(finished);
  service.Shutdown();
  assert(ps5::TryHoldEffectiveUid());
  std::filesystem::remove_all(path);
}
