// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/runtime_host.hpp"
#include <functional>
#include <optional>
namespace ww::steam_integration {
// Reads the installed Playden kit and verifies its actual files before use.
// It does not modify, update, or replace that kit.
std::function<std::optional<playden::homebrew::RuntimeKit>(std::string*)> InstalledKitAdmission();
}
