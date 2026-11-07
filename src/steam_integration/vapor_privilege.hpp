// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace ww::steam_integration {
// ww::privilege raiser for the Vapor build. Uses Playden's credential lock so
// a desktop step never raises root in the middle of a backend batch; when the
// backend holds the lock the step fails at once with EBUSY instead of
// blocking the UI.
int VaporRaise(int (*step)(void *), void *context);
}
