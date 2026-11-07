// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/native_startup.hpp"
#include "../src/steam_integration/native_session.hpp"
#include "../src/steam_integration/native_bridge.hpp"
#include "../src/steam_ui.hpp"
#include "../src/shell.hpp"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {
std::mutex gate_mutex;
std::condition_variable gate;
bool entered = false, release_worker = false, fail = false, fail_trust = false, host_enabled = false, default_locations_used = false;
std::vector<int> teardown;
struct FakeUi : ww::SteamUi {
  void tick() override {} ww::SteamStatus status() const override { return {}; }
  bool game(std::size_t, ww::SteamGame&) const override { return false; }
  bool qr_module(int, int) const override { return false; }
  void sign_in() override {} void cancel() override {} void sign_out() override {} void refresh() override {}
} fake_ui;
}
namespace ww::steam_integration {
class NativeSession::Impl {};
NativeSession::NativeSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
NativeSession::~NativeSession() { if (host_enabled) teardown.push_back(2); }
std::unique_ptr<NativeSession> NativeSession::Create(const char*, int fd, const char*, int data_fd) {
  assert(fcntl(fd, F_GETFD) >= 0);
  if (data_fd >= 0) assert(fcntl(data_fd, F_GETFD) >= 0);
  std::unique_lock<std::mutex> lock(gate_mutex); entered = true; gate.notify_all();
  gate.wait(lock, [] { return release_worker; });
  if (data_fd >= 0) assert(fcntl(data_fd, F_GETFD) >= 0);
  if (fail) return {};
  return std::unique_ptr<NativeSession>(new NativeSession(std::make_unique<Impl>()));
}
ww::SteamUi& NativeSession::ui() { return fake_ui; }
bool NativeSession::AttachReadyLaunch(ReadyLaunch*) { return false; }
bool NativeSession::AttachGameRuntime(playden::homebrew::RuntimeHost&, CloudGate) { return false; }
CloudPreflight* NativeSession::EnableCloud() { return nullptr; }
bool NativeSession::AttachGameRuntimeWithCloud(playden::homebrew::RuntimeHost&, CloudLocationResolver) { return host_enabled; }
CloudLocationResolver NativeSession::CloudLocations(playden::homebrew::RuntimeHost&) {
  default_locations_used = true;
  return [](std::uint32_t) -> std::optional<playden::steam::CloudLocation> { return std::nullopt; };
}
std::optional<playden::homebrew::RuntimeHostConfig> NativeSession::GameHostConfig(
    std::string, CoordinatedTitleList&, playden::ps5::TitleRegistry::Transport, NativeTitleLauncher) {
  if (!host_enabled) return std::nullopt;
  return playden::homebrew::RuntimeHostConfig{};
}
bool VerifyPackagedSteamCa(const char*) { std::lock_guard<std::mutex> lock(gate_mutex); return !fail_trust; }
}
namespace playden::homebrew {
RuntimeHost::RuntimeHost(RuntimeHostConfig config) : config_(std::move(config)) {}
RuntimeHost::~RuntimeHost() { if (host_enabled) teardown.push_back(3); }
bool RuntimeHost::Start() { return host_enabled; }
void RuntimeHost::Shutdown() { if (host_enabled) teardown.push_back(1); }
}
namespace playden::runtime {
std::vector<VcRedist> MicrosoftVcRedists() { return {}; }
const std::vector<std::filesystem::path>& PrivateProfilePaths() { static const std::vector<std::filesystem::path> value; return value; }
}
struct Titles : ww::steam_integration::CoordinatedTitleList {
  std::optional<std::string> Read() override { return std::string{}; }
  ww::steam_integration::TitleListReplace ReplaceIfUnchanged(const std::string&, const std::string&) override { return ww::steam_integration::TitleListReplace::written; }
};

int main() {
  using namespace ww::steam_integration;
  int anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  int data_anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(data_anchor >= 0);
  NativeStartup startup;
  assert(startup.Poll() == NativeStartupState::idle && !startup.ui());
  assert(startup.Start("/tmp", anchor, "/verified-ca", data_anchor));
  assert(close(anchor) == 0);  // worker owns a duplicate
  assert(close(data_anchor) == 0);
  { std::unique_lock<std::mutex> lock(gate_mutex);
    assert(gate.wait_for(lock, std::chrono::seconds(2), [] { return entered; })); }
  assert(startup.Poll() == NativeStartupState::starting && !startup.ui());
  assert(!startup.Start("/tmp", 0, "/verified-ca"));
  { std::lock_guard<std::mutex> lock(gate_mutex); release_worker = true; } gate.notify_all();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (startup.Poll() == NativeStartupState::starting && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  assert(startup.Poll() == NativeStartupState::ready && startup.ui() == &fake_ui);
  { std::lock_guard<std::mutex> lock(gate_mutex); entered = false; release_worker = true; fail = true; }
  anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  NativeStartup failed;
  assert(failed.Start("/tmp", anchor, "/verified-ca")); close(anchor);
  const auto deadline2 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (failed.Poll() == NativeStartupState::starting && std::chrono::steady_clock::now() < deadline2)
    std::this_thread::yield();
  assert(failed.Poll() == NativeStartupState::failed && !failed.ui());
  { std::lock_guard<std::mutex> lock(gate_mutex); entered = false; fail_trust = true; }
  anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  NativeStartup untrusted;
  assert(untrusted.Start("/tmp", anchor, "/bad-ca")); close(anchor);
  const auto deadline3 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (untrusted.Poll() == NativeStartupState::starting && std::chrono::steady_clock::now() < deadline3)
    std::this_thread::yield();
  assert(untrusted.Poll() == NativeStartupState::failed && !untrusted.ui());
  { std::lock_guard<std::mutex> lock(gate_mutex); assert(!entered); }
  { std::lock_guard<std::mutex> lock(gate_mutex); fail = false; fail_trust = false; }
  ww::Shell shell;
  anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    NativeBridge bridge(shell);
    assert(bridge.Start("/tmp", anchor, "/verified-ca")); close(anchor);
    const auto deadline4 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (bridge.Tick() == NativeStartupState::starting && std::chrono::steady_clock::now() < deadline4)
      std::this_thread::yield();
    assert(bridge.Tick() == NativeStartupState::ready && shell.steam == &fake_ui);
  }
  assert(!shell.steam);
  host_enabled = true; teardown.clear();
  anchor = open("/tmp", O_RDONLY | O_DIRECTORY); assert(anchor >= 0);
  {
    Titles titles; NativeBridge bridge(shell);
    assert(bridge.Start("/tmp", anchor, "/verified-ca")); close(anchor);
    const auto deadline5 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (bridge.Tick() == NativeStartupState::starting && std::chrono::steady_clock::now() < deadline5)
      std::this_thread::yield();
    assert(bridge.Tick() == NativeStartupState::ready);
    auto registry = [](const std::string&, const std::string&) -> std::optional<std::string> { return "ok"; };
    auto launch = [](const std::string&, const std::vector<std::string>&) { return playden::ps5::TitleResult{}; };
    auto locations = [](std::uint32_t) -> std::optional<playden::steam::CloudLocation> { return std::nullopt; };
    assert(bridge.StartGameHost("/app0/runtime", titles, registry, launch, {}));
    assert(default_locations_used);
    assert(!bridge.StartGameHost("/app0/runtime", titles, registry, launch, locations));
  }
  assert((teardown == std::vector<int>{1, 2, 3}));
  host_enabled = false;
  std::puts("PASS: startup is asynchronous, retains its anchor and exposes UI only after success");
}
