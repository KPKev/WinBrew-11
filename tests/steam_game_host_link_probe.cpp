// SPDX-License-Identifier: GPL-3.0-or-later
// Link-only research executable. It must never be run or deployed.
#include "homebrew/runtime_host.hpp"
#include <utility>
int main() {
  playden::homebrew::RuntimeHostConfig config;
  playden::homebrew::RuntimeHost host(std::move(config));
  (void)host.Start(); host.Prepare({}); host.Play(0); (void)host.Handoff();
}
