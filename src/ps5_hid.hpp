// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace ww {
struct Shell;
}
namespace native_input {
using Log = void (*)(const char *, unsigned);
// Load only: call before elevation; device Init/Open follows elevation.
void preload_modules(Log);
void open(int user, Log);
void poll(ww::Shell &, double now);
void close();
} // namespace native_input
