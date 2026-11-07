// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "title_absence.hpp"
#include "homebrew/runtime_host.hpp"
namespace ww::steam_integration {
// Worker only. Unknown is returned for any unavailable inventory or path.
TitleObservation VerifyNativeTitleAbsent(playden::homebrew::RuntimeHost& host, const std::string& title);
}
