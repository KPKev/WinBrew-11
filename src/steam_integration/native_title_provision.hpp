// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native_game_startup.hpp"
namespace ww::steam_integration {
// Duplicate the independently opened console /data descriptor. The returned
// worker callback provisions Wine 11's root-owned list under the UID guard.
NativeGameStartup::Provision MakeConsoleTitleProvision(int data_anchor);
}
