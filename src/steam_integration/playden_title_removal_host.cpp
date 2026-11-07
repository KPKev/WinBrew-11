// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_removal.hpp"
namespace ww::steam_integration {
std::shared_ptr<const playden::homebrew::RuntimeSnapshot> PlaydenTitleRemovalHost::Snapshot() const { return host_.Shared(); }
void PlaydenTitleRemovalHost::Remove(std::uint32_t app) { host_.Remove(app, false); }
}
