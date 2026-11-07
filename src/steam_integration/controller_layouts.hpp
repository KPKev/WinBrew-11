// SPDX-License-Identifier: GPL-3.0-or-later
// Per-game controller layouts for Vapor, applied through Playden's own game
// settings (runtime::GameSettingsRecord overrides) and its kApplySettings
// maintenance step. Nothing here talks to the console directly.
#pragma once
#include "runtime/game_settings.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace ww::steam_integration {
enum class ControllerLayout {
  gamepad,          // Playden default: the DualSense is XInput controller 0, no pointer
  gamepad_pointer,  // as gamepad, plus right stick moves the pointer and touchpad click is a left click
  mouse,            // Playden "mouse" preset: left stick pointer, Cross click, Circle right click
  arrows,           // Playden "arrows" preset: d-pad arrows, Cross Z, Circle X, Triangle C
};
constexpr int kControllerLayoutCount = 4;
const char *ControllerLayoutName(ControllerLayout layout);         // for the UI
const char *ControllerLayoutDescription(ControllerLayout layout);  // one line, for the UI
ControllerLayout NextControllerLayout(ControllerLayout layout);

// Playden settings overrides for a layout (ids from runtime/game_settings.cpp).
std::map<std::string, std::string> ControllerLayoutOverrides(ControllerLayout layout);

// Durable per-game choice. The file lives in Vapor's private state folder and
// is replaced atomically (temporary file, fsync, rename, directory fsync).
class ControllerLayoutStore {
 public:
  explicit ControllerLayoutStore(std::filesystem::path state_folder);
  bool Load();  // false: unreadable or malformed (the store stays empty, defaults apply)
  ControllerLayout Get(std::uint32_t app) const;
  // Saves a new choice and returns its settings revision (0 on failure).
  std::uint64_t Set(std::uint32_t app, ControllerLayout layout);
  // The record Playden asks for in RuntimeHostConfig::settings_record; nothing
  // when the player never chose a layout for this game (Playden defaults),
  // except Skyrim, which supplies a versioned program/gamepad configuration.
  std::optional<playden::runtime::GameSettingsRecord> Record(std::uint32_t app) const;

 private:
  struct Entry { ControllerLayout layout = ControllerLayout::gamepad; std::uint64_t revision = 0; };
  bool SaveLocked() const;
  std::filesystem::path folder_;
  mutable std::mutex mutex_;
  std::map<std::uint32_t, Entry> entries_;
};
}  // namespace ww::steam_integration
