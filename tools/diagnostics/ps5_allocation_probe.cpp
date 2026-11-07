// SPDX-License-Identifier: GPL-3.0-or-later
// Keep the boilerplate renderer's allocation observation in the link probe.
extern "C" __attribute__((noinline, visibility("hidden"))) bool
ps5ObserveOwnedAllocation(const void* address) noexcept {
  __asm__ volatile("" : : "r"(address) : "memory");
  return address != nullptr;
}
