// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/guarded_runner.hpp"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <unistd.h>
struct Delegate final : playden::installs::InstallRunner {
  int installs=0, removals=0, forgotten=0;
  std::atomic<bool> hold{false}, entered{false}, fail{false};
  Result Install(const playden::GameId&, const std::string&, const std::atomic<bool>& cancel,
    const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
    const std::function<void(playden::installs::InstallStep)>&) override {
      ++installs; ww::steam_integration::traffic::received(123); entered = true; while (hold) std::this_thread::yield(); if (fail) throw std::runtime_error("test"); if (cancel) return {false,true,"",0,false,""}; return {true,false,"",10,false,""};
    }
  std::string Remove(const std::string&) override { ++removals; return {}; }
  void Forget(const playden::GameId&) override { ++forgotten; }
};
int main() {
  char path[] = "/tmp/wine11-guard-test-XXXXXX"; assert(mkdtemp(path));
  const std::string root = std::string(path) + "/wine11-games"; assert(mkdir(root.c_str(),0700)==0);
  const std::string lock_path = root + "/wine11-install.lock";
  int fd=open(lock_path.c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600); assert(fd>=0); assert(close(fd)==0);
  {
    using namespace playden::platform;
    auto directory=ManagedDirectory::Open(root, DirectoryPolicy::kPrivate); assert(directory);
    auto lease=ManagedFileLock::Acquire(*directory,"wine11-install.lock",LockMode::kPublisher); assert(lease);
    assert(!ManagedFileLock::Acquire(*directory,"wine11-install.lock",LockMode::kPublisher));
    Delegate delegate; ww::steam_integration::traffic::Registry traffic;
    ww::steam_integration::GuardedInstallRunner guard(*directory,*lease,delegate,&traffic);
    std::atomic<bool> cancel{false}; auto id=playden::GameId::Steam(440);
    auto progress=[](std::uint64_t,std::uint64_t,std::uint64_t){};
    auto step=[](playden::installs::InstallStep){};
    assert(guard.Install(id,root+"/440",cancel,progress,step).ok && delegate.installs==1);
    assert(!guard.Install(id,root+"/441",cancel,progress,step).ok && delegate.installs==1);
    assert(!guard.Remove(root+"/..").empty() && !guard.Remove(root+"/0440").empty() && delegate.removals==0);
    assert(guard.Remove(root+"/440").empty() && delegate.removals==1);
    delegate.hold = true; delegate.entered = false;
    std::thread transfer([&] { assert(guard.Install(id,root+"/440",cancel,progress,step).ok); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!delegate.entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(delegate.entered && !playden::ps5::TryHoldEffectiveUid());
    auto observed = traffic.get(440); assert(observed && observed->bytes.load() == 123 && !traffic.get(441));
    delegate.hold = false; transfer.join();
    assert(!traffic.get(440)); // Completed or canceled attempts cannot leak attribution.
    assert(playden::ps5::TryHoldEffectiveUid());
    delegate.hold = true; delegate.entered = false;
    std::thread canceled_transfer([&] { assert(guard.Install(id,root+"/440",cancel,progress,step).canceled); });
    const auto cancel_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!delegate.entered && std::chrono::steady_clock::now() < cancel_deadline) std::this_thread::yield();
    assert(delegate.entered && traffic.get(440)); cancel = true; delegate.hold = false; canceled_transfer.join();
    assert(!traffic.get(440)); cancel = false;
    delegate.fail = true;
    try { guard.Install(id,root+"/440",cancel,progress,step); assert(false); }
    catch (const std::runtime_error&) { }
    assert(!traffic.get(440) && !ww::steam_integration::traffic::current);
    delegate.fail = false;
    assert(rename(lock_path.c_str(),(root+"/old.lock").c_str())==0);
    fd=open(lock_path.c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600); assert(fd>=0); assert(close(fd)==0);
    assert(!guard.Install(id,root+"/440",cancel,progress,step).ok);
    assert(!guard.Remove(root+"/440").empty()); guard.Forget(id);
    assert(delegate.installs==4 && delegate.removals==1 && delegate.forgotten==0);
  }
  std::filesystem::remove_all(path);
  std::puts("PASS: exclusive game lease, canonical game paths, identity binding and replaced-lock refusal");
}
