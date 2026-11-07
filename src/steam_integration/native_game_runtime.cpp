// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_game_runtime.hpp"
#include "runtime.hpp"
#include <utility>
namespace ww::steam_integration {
NativeGameRuntime::NativeGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud)
    : host_(host), preparation_host_(host), launch_host_(host), preparation_(preparation_host_),
      launch_(launch_host_, std::move(cloud)) {}
NativeGameRuntime::NativeGameRuntime(playden::homebrew::RuntimeHost& host, CloudPreflight& cloud, CloudLocationResolver resolver)
    : host_(host), preparation_host_(host), launch_host_(host), preparation_(preparation_host_),
      cloud_gate_(std::make_unique<CloudLaunchGate>(cloud, std::move(resolver))),
      launch_(launch_host_,
        [this](std::uint32_t app, std::uint64_t account, std::uint64_t generation) {
          return cloud_gate_->Poll(app, account, generation);
        },
        [this](std::uint32_t app, const std::string& root, std::uint64_t account, std::uint64_t generation, std::string* error) {
          return cloud_gate_->Begin(app, root, account, generation, error);
        }) {}
bool NativeGameRuntime::Attach(Runtime& runtime) {
  if (!runtime.AttachGameRuntime(&launch_, &preparation_)) return false;
#if defined(WINE11_EXPERIMENT_UNINSTALL)
  (void)runtime.AttachGameRemoval(host_); // Existing launch remains available if removal admission fails.
#endif
  return true;
}
}
