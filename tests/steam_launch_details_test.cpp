// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/launch_details.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
using namespace playden;
using namespace playden::steam;
using namespace ww::steam_integration;
struct Sessions : SessionRepository {
  Result<std::optional<StoredSession>> Load() const override { return Result<std::optional<StoredSession>>::Success(StoredSession{42,"Player","r","a",{}}); }
  Result<void> Save(const StoredSession&) const override { return Result<void>::Success(); }
  Result<void> Clear() const override { return Result<void>::Success(); }
};
struct Source : AppDetailsSource {
  std::atomic<bool> hold{false}, entered{false}, canceled{false}, missing{false};
  std::vector<AppDetails> Fetch(const StoredSession&, const std::vector<GameId>& ids, const std::atomic<bool>& cancel) override {
    entered = true;
    while (hold && !cancel) std::this_thread::yield();
    if (cancel) { canceled = true; return {}; }
    if (missing) return {};
    AppDetails details; details.id = ids.at(0); details.available = true; details.launches_any_branch = true;
    details.launches.push_back({"game.exe", "-play", "bin", "Play game"}); return {details};
  }
};
int main() {
  Sessions sessions; Source source; AppDetailsService service(source, sessions); LaunchDetails details(service);
  homebrew::RuntimePrepareRequest request; request.game = GameId::Steam(440); request.runtime_number = 440;
  request.install_path = "/games/440"; request.volume_id = "internal";
  assert(details.Request(request, 42, 3));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (details.state() == LaunchDetailsState::waiting && std::chrono::steady_clock::now() < deadline) {
    details.Tick(42, 3); std::this_thread::yield();
  }
  assert(details.state() == LaunchDetailsState::ready && details.request().launches.size() == 1);
  assert(details.request().launches[0].executable == "game.exe" && details.request().launches_any_branch);
  assert(details.Request(request, 42, 4)); details.Tick(43, 5);
  assert(details.state() == LaunchDetailsState::failed);
  source.entered = false; source.hold = true;
  assert(details.Request(request, 42, 6));
  const auto deadline2 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!source.entered && std::chrono::steady_clock::now() < deadline2) std::this_thread::yield();
  assert(source.entered); details.Cancel();
  assert(details.state() == LaunchDetailsState::failed && details.message() == "Game details request canceled.");
  while (!source.canceled && std::chrono::steady_clock::now() < deadline2) std::this_thread::yield();
  assert(source.canceled);
  source.hold = false; source.missing = true;
  assert(details.Request(request, 42, 7));
  const auto deadline3 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (details.state() == LaunchDetailsState::waiting && std::chrono::steady_clock::now() < deadline3) {
    details.Tick(42, 7); std::this_thread::yield();
  }
  assert(details.state() == LaunchDetailsState::ready && details.request().launches.empty());
  assert(!details.request().launch_reason.empty());
  std::puts("PASS: Steam launch options are retrieved asynchronously and account-bound");
}
