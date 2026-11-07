// SPDX-License-Identifier: GPL-3.0-or-later
#include "preparation.hpp"
#include <utility>
namespace ww::steam_integration {
std::shared_ptr<const playden::homebrew::RuntimeSnapshot> PlaydenPreparationHost::Snapshot() const { return host_.Shared(); }
void PlaydenPreparationHost::Prepare(playden::homebrew::RuntimePrepareRequest request) { host_.Prepare(std::move(request)); }
}
