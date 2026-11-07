// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace native_browser {
using Log = void (*)(const char *, unsigned);
// Load system modules before elevation, initialize their services afterward.
void preload_modules(Log);
void initialize_dialog();
int open(const char *url, int user);
bool active();
void poll();
void shutdown();
} // namespace native_browser
