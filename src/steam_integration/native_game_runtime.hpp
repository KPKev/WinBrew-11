// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "preparation.hpp"
#include "ready_launch.hpp"
#include "cloud_launch_gate.hpp"
namespace ww::steam_integration {
class Runtime;
// Owns the app-thread adapters for an externally configured Playden host.
// The host and cloud gate dependencies must outlive this object.
class NativeGameRuntime {
 public:
  NativeGameRuntime(playden::homebrew::RuntimeHost& host, CloudGate cloud);
  NativeGameRuntime(playden::homebrew::RuntimeHost& host, CloudPreflight& cloud, CloudLocationResolver resolver);
  bool Attach(Runtime& runtime);
 private:
  playden::homebrew::RuntimeHost& host_;
  PlaydenPreparationHost preparation_host_;
  PlaydenReadyLaunchHost launch_host_;
  Preparation preparation_;
  std::unique_ptr<CloudLaunchGate> cloud_gate_;
  ReadyLaunch launch_;
};
}
