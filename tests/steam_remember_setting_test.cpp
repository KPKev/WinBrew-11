// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/steam_integration/remember_setting.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
int main() {
  using namespace ww::steam_integration;
  std::atomic<bool> entered{false}, release{false};
  RememberSetting setting(false, [&](bool enabled) {
    entered = true; while (!release) std::this_thread::yield(); return enabled;
  });
  assert(setting.Set(true));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(entered && setting.View().busy && !setting.Set(false));
  release = true;
  while (setting.View().busy && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(setting.View().enabled && !setting.View().failed);
  assert(setting.Set(false));
  while (setting.View().busy && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(setting.View().failed && setting.View().enabled);
}
