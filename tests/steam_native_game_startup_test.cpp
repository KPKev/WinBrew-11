// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_game_startup.hpp"
#include "../src/steam_integration/native_session.hpp"
#include "../src/shell.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>
namespace {
std::atomic<bool> entered{false}, release{false};
bool host_started = false;
bool release_on_bridge_destroy = false;
std::vector<int> teardown;
struct Titles : ww::steam_integration::CoordinatedTitleList {
  ~Titles() override { teardown.push_back(2); }
  std::optional<std::string> Read() override { return {}; }
  ww::steam_integration::TitleListReplace ReplaceIfUnchanged(const std::string&, const std::string&) override {
    return ww::steam_integration::TitleListReplace::unavailable;
  }
};
}
namespace ww::steam_integration {
class NativeSession::Impl {};
NativeSession::~NativeSession() = default;
NativeStartup::NativeStartup() = default;
NativeStartup::~NativeStartup() = default;
NativeBridge::NativeBridge(ww::Shell& shell) : shell_(shell) {}
NativeBridge::~NativeBridge() { teardown.push_back(1); if (release_on_bridge_destroy) release = true; }
bool NativeBridge::Start(const char*, int, const char*, int) { return true; }
NativeStartupState NativeBridge::Tick() { return NativeStartupState::ready; }
bool NativeBridge::StartGameHost(std::string dir, CoordinatedTitleList&, playden::ps5::TitleRegistry::Transport,
                                 NativeTitleLauncher, CloudLocationResolver resolver) {
  assert(dir == "/app0/runtime" && !resolver); host_started = true; return true;
}
}
namespace playden::homebrew {
RuntimeHost::~RuntimeHost() = default;
}
int main() {
  using namespace ww::steam_integration;
  ww::Shell shell;
  auto registry = [](const std::string&, const std::string&) -> std::optional<std::string> { return {}; };
  auto launch = [](const std::string&, const std::vector<std::string>&) { return playden::ps5::TitleResult{}; };
  {
    NativeGameStartup startup(shell);
    assert(startup.Start("/state", 1, "/ca", "/app0/runtime", [] {
      entered = true; while (!release) std::this_thread::yield();
      return std::make_unique<Titles>();
    }, registry, launch));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    assert(startup.Tick() == GameStartupState::preparing);
    while (!entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(entered && startup.Tick() == GameStartupState::preparing && !host_started);
    release = true;
    while (startup.Tick() == GameStartupState::preparing && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    assert(startup.Tick() == GameStartupState::ready && host_started);
  }
  assert((teardown == std::vector<int>{1, 2}));
  teardown.clear(); host_started = false;
  {
    NativeGameStartup startup(shell);
    assert(startup.Start("/state", 1, "/ca", "/app0/runtime", []() -> std::unique_ptr<CoordinatedTitleList> {
      return {};
    }, registry, launch));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (startup.Tick() == GameStartupState::preparing && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    assert(startup.Tick() == GameStartupState::failed && !host_started);
  }
  assert((teardown == std::vector<int>{1}));
  teardown.clear(); entered = false; release = false; release_on_bridge_destroy = true;
  {
    NativeGameStartup startup(shell);
    assert(startup.Start("/state", 1, "/ca", "/app0/runtime", [] {
      entered = true; while (!release) std::this_thread::yield();
      return std::make_unique<Titles>();
    }, registry, launch));
    assert(startup.Tick() == GameStartupState::preparing);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(entered);
  }
  assert((teardown == std::vector<int>{1, 2}));
  release_on_bridge_destroy = false;
  teardown.clear();
  {
    NativeGameStartup startup(shell);
    assert(startup.Start("/state", 1, "/ca", "/app0/runtime", {}, registry, launch));
    assert(startup.Tick() == GameStartupState::failed);
  }
  assert((teardown == std::vector<int>{1}));
}
