// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Keep the current installed backend buildable while the isolated 0.11 port is
// qualified. A new integration must still pass the matched-package ABI gate.
#if __has_include("runtime/steam_host.h")
#define WW_PLAYDEN_BACKEND_011 1
#else
#define WW_PLAYDEN_BACKEND_011 0
#endif

#if __has_include("vapor_backend_version.h")
#include "vapor_backend_version.h"
#elif WW_PLAYDEN_BACKEND_011
#define WW_PLAYDEN_BACKEND_VERSION "0.11.0"
#else
#define WW_PLAYDEN_BACKEND_VERSION "0.8.0"
#endif
