// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/guarded_runner.hpp"
#include "steam/cloud_service.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
using namespace playden;
struct Sessions : SessionRepository {
  Result<std::optional<StoredSession>> Load() const override { return Result<std::optional<StoredSession>>::Success(StoredSession{42,"Player","r","a",{}}); }
  Result<void> Save(const StoredSession&) const override { return Result<void>::Success(); }
  Result<void> Clear() const override { return Result<void>::Success(); }
};
struct Cloud : steam::CloudRunner {
  std::atomic<bool> entered{false}, release{false};
  steam::CloudReport Check(const StoredSession&, const steam::CloudLocation&, const std::atomic<bool>& cancel) override {
    entered = true; while (!release && !cancel) std::this_thread::yield();
    steam::CloudReport out; out.state = steam::CloudState::kUpToDate; return out;
  }
  steam::CloudReport Apply(const StoredSession&, const steam::CloudLocation&, steam::CloudResolution, bool,
                           const std::atomic<bool>&) override { return {}; }
};
struct FakeInstaller : installs::InstallRunner {
  std::atomic<bool> entered{false};
  Result Install(const GameId&, const std::string&, const std::atomic<bool>&,
                 const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
                 const std::function<void(installs::InstallStep)>&) override {
    entered = true; return {true,false,"",0,false,{}};
  }
  std::string Remove(const std::string&) override { return {}; }
};
int main() {
  char path[] = "/tmp/wine11-uid-workers-XXXXXX"; assert(mkdtemp(path));
  const auto dir = std::filesystem::path(path) / "wine11-games";
  assert(std::filesystem::create_directory(dir)); assert(chmod(dir.c_str(), 0700) == 0);
  int fd = open((dir / "wine11-install.lock").c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600); assert(fd >= 0); close(fd);
  auto directory = platform::ManagedDirectory::Open(dir, platform::DirectoryPolicy::kPrivate); assert(directory);
  auto lease = platform::ManagedFileLock::Acquire(*directory, "wine11-install.lock", platform::LockMode::kPublisher); assert(lease);
  FakeInstaller delegate; ww::steam_integration::GuardedInstallRunner install(*directory, *lease, delegate);
  Sessions sessions; Cloud cloud; steam::CloudService service(cloud, sessions);
  steam::CloudLocation location{GameId::Steam(440), path, path, path}; assert(service.Sync(location));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!cloud.entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(cloud.entered);
  std::atomic<bool> cancel{false}, launched{false};
  std::thread worker([&] {
    launched = true;
    assert(install.Install(GameId::Steam(440), (dir/"440").string(), cancel,
                           [](auto,auto,auto){}, [](auto){}).ok);
  });
  while (!launched && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(launched && !delegate.entered);
  cloud.release = true;
  worker.join(); assert(delegate.entered);
  service.Shutdown(); lease.reset(); directory.reset(); std::filesystem::remove_all(path);
}
