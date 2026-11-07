// SPDX-License-Identifier: GPL-3.0-or-later
#include "privilege.hpp"
#include <atomic>

namespace ww::privilege {
namespace {
std::atomic<Raise> current{nullptr};
}
void install(Raise raise) { current.store(raise, std::memory_order_release); }
bool installed() { return current.load(std::memory_order_acquire) != nullptr; }
int run_step(int (*step)(void *), void *context) {
  const Raise raise = current.load(std::memory_order_acquire);
  return raise ? raise(step, context) : step(context);
}
}  // namespace ww::privilege
