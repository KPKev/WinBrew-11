// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
namespace ww::steam_integration::traffic {
struct Counter { std::atomic<std::uint64_t> bytes{0}; std::atomic<bool> observed{false}; };
class Scope;
// A raw scope pointer has no TLS destructor. Every scope explicitly owns its
// counter, and nested scopes restore the previous pointer before destruction.
inline thread_local Scope* current = nullptr;
class Scope {
 public:
  explicit Scope(std::shared_ptr<Counter> counter) : previous_(current), counter_(std::move(counter)) { current = this; }
  ~Scope() { current = previous_; }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  const std::shared_ptr<Counter>& counter() const { return counter_; }
 private:
  Scope* previous_;
  std::shared_ptr<Counter> counter_;
};
// Copy ownership on the install thread before starting any fetch workers.
inline std::shared_ptr<Counter> capture() { return current ? current->counter() : nullptr; }
inline void received(std::uint64_t count) noexcept {
  if (!current || !current->counter() || !count) return;
  auto& counter = *current->counter();
  counter.observed.store(true, std::memory_order_release);
  auto& bytes = counter.bytes;
  auto old = bytes.load(std::memory_order_relaxed);
  do {
    const auto max = std::numeric_limits<std::uint64_t>::max();
    const auto next = count > max - old ? max : old + count;
    if (bytes.compare_exchange_weak(old, next, std::memory_order_relaxed)) break;
  } while (true);
}
// One active install attempt; the registry outlives the runner and UI.
class Registry {
 public:
  std::shared_ptr<Counter> begin(std::uint32_t app) {
    auto counter = std::make_shared<Counter>();
    std::lock_guard<std::mutex> lock(mutex_); app_ = app; active_ = counter; return counter;
  }
  void end(const std::shared_ptr<Counter>& counter) {
    std::lock_guard<std::mutex> lock(mutex_); if (active_ == counter) { active_.reset(); app_ = 0; }
  }
  std::shared_ptr<Counter> get(std::uint32_t app) {
    std::lock_guard<std::mutex> lock(mutex_); return app && app == app_ ? active_ : nullptr;
  }
 private:
  std::mutex mutex_; std::uint32_t app_ = 0; std::shared_ptr<Counter> active_;
};
class Sampler {
 public:
  std::optional<std::uint64_t> sample(const std::shared_ptr<Counter>& c, std::uint64_t now) {
    if (!c || !c->observed.load(std::memory_order_acquire)) { reset(); return {}; }
    const auto bytes = c->bytes.load(std::memory_order_relaxed);
    if (last_ != c || now < last_ms_) { last_ = c; last_ms_ = now; last_bytes_ = bytes; value_.reset(); return {}; }
    const auto elapsed = now - last_ms_;
    if (elapsed < 1000) return value_;
    const auto delta = bytes - last_bytes_;
    const long double rate = static_cast<long double>(delta) * 1000 / elapsed;
    const auto max = std::numeric_limits<std::uint64_t>::max();
    value_ = rate >= static_cast<long double>(max) ? max : static_cast<std::uint64_t>(rate);
    last_ms_ = now; last_bytes_ = bytes; return value_;
  }
  void reset() { last_.reset(); value_.reset(); }
 private:
  std::shared_ptr<Counter> last_; std::uint64_t last_ms_ = 0, last_bytes_ = 0;
  std::optional<std::uint64_t> value_;
};
}
