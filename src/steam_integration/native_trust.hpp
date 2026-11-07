// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace ww::steam_integration {
// Only call with a CA path in the immutable packaged application mount. The
// file is checked against the bundle used by the console network probes.
bool VerifyPackagedSteamCa(const char* path);
}
