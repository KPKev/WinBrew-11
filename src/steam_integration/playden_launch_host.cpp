// SPDX-License-Identifier: GPL-3.0-or-later
#include "ready_launch.hpp"
namespace ww::steam_integration {
std::shared_ptr<const playden::homebrew::RuntimeSnapshot> PlaydenReadyLaunchHost::Snapshot() const { return host_.Shared(); }
void PlaydenReadyLaunchHost::Play(std::uint32_t app) { host_.Play(app); }
bool PlaydenReadyLaunchHost::Handoff() { return host_.Handoff(); }
}
