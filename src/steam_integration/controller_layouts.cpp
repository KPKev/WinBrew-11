// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller_layouts.hpp"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
constexpr const char *kFile = "controller-layouts.v1";
constexpr const char *kHeader = "wine11-controller-layouts 1";
// Steam's own launcher cannot start Skyrim SE on this runtime: Playden reports
// invalid handle from its ShellExecuteEx. Upstream's tested configuration starts
// the game's executable directly (Playden COMPATIBILITY.md, PR #408).
constexpr std::uint32_t kSkyrimSpecialEdition = 489830;
constexpr const char *kSkyrimProgram = "SkyrimSE.exe";
// Controller choices were already published at revisions 1, 2, ... before
// Vapor supplied a Skyrim program. A schema range forces those choices to
// republish with the executable change, even when the layout is unchanged.
constexpr std::uint64_t kSkyrimSettingsEpoch = 1ull << 32;

bool ParseLayout(const std::string &text, ControllerLayout &out) {
  if (text == "gamepad") out = ControllerLayout::gamepad;
  else if (text == "gamepad_pointer") out = ControllerLayout::gamepad_pointer;
  else if (text == "mouse") out = ControllerLayout::mouse;
  else if (text == "arrows") out = ControllerLayout::arrows;
  else return false;
  return true;
}
const char *LayoutKey(ControllerLayout layout) {
  switch (layout) {
    case ControllerLayout::gamepad: return "gamepad";
    case ControllerLayout::gamepad_pointer: return "gamepad_pointer";
    case ControllerLayout::mouse: return "mouse";
    case ControllerLayout::arrows: return "arrows";
  }
  return "gamepad";
}
bool ParseNumber(const std::string &text, unsigned long long &out) {
  if (text.empty() || text.size() > 20) return false;
  unsigned long long value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    const unsigned long long digit = static_cast<unsigned long long>(c - '0');
    if (value > (~0ull - digit) / 10) return false;
    value = value * 10 + digit;
  }
  out = value;
  return true;
}
bool WriteAll(int fd, const std::string &bytes) {
  size_t done = 0;
  while (done < bytes.size()) {
    const ssize_t n = write(fd, bytes.data() + done, bytes.size() - done);
    if (n < 0) { if (errno == EINTR) continue; return false; }
    if (n == 0) return false;
    done += static_cast<size_t>(n);
  }
  return true;
}
}  // namespace

const char *ControllerLayoutName(ControllerLayout layout) {
  switch (layout) {
    case ControllerLayout::gamepad: return "Controller";
    case ControllerLayout::gamepad_pointer: return "Controller + pointer";
    case ControllerLayout::mouse: return "Mouse";
    case ControllerLayout::arrows: return "Arrow keys";
  }
  return "Controller";
}
const char *ControllerLayoutDescription(ControllerLayout layout) {
  switch (layout) {
    case ControllerLayout::gamepad: return "Xbox controller layout for games with controller support.";
    case ControllerLayout::gamepad_pointer: return "Xbox layout; right stick moves the pointer, touchpad click clicks. For launchers and menus.";
    case ControllerLayout::mouse: return "Left stick moves the pointer; Cross clicks, Circle right-clicks, Options Enter.";
    case ControllerLayout::arrows: return "D-pad sends arrow keys; Cross Z, Circle X, Triangle C, Options Enter.";
  }
  return "";
}
ControllerLayout NextControllerLayout(ControllerLayout layout) {
  return static_cast<ControllerLayout>((static_cast<int>(layout) + 1) % kControllerLayoutCount);
}

std::map<std::string, std::string> ControllerLayoutOverrides(ControllerLayout layout) {
  switch (layout) {
    case ControllerLayout::gamepad:
      return {{"controller", "xinput"}, {"controller_preset", "none"}, {"pointer_stick", "none"}};
    case ControllerLayout::gamepad_pointer:
      // The touchpad has no XInput equivalent, so clicking with it never
      // collides with a game's A/B/X/Y or stick buttons.
      return {{"controller", "xinput"}, {"controller_preset", "none"}, {"pointer_stick", "right_stick"},
              {"button.touchpad", "mouse_left"}};
    case ControllerLayout::mouse:
      return {{"controller_preset", "mouse"}};
    case ControllerLayout::arrows:
      return {{"controller_preset", "arrows"}};
  }
  return {};
}

ControllerLayoutStore::ControllerLayoutStore(std::filesystem::path state_folder) : folder_(std::move(state_folder)) {}

bool ControllerLayoutStore::Load() {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.clear();
  const auto path = folder_ / kFile;
  const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return errno == ENOENT;
  std::string bytes;
  char buffer[4096];
  for (;;) {
    const ssize_t n = read(fd, buffer, sizeof buffer);
    if (n < 0) { if (errno == EINTR) continue; close(fd); return false; }
    if (n == 0) break;
    bytes.append(buffer, static_cast<size_t>(n));
    if (bytes.size() > 1u << 20) { close(fd); return false; }
  }
  close(fd);
  // Plain parsing: no iostreams (they pull libc++ locale code the console's
  // libc does not provide).
  std::map<std::uint32_t, Entry> parsed;
  size_t line_start = 0;
  bool header = true;
  while (line_start < bytes.size()) {
    size_t line_end = bytes.find('\n', line_start);
    if (line_end == std::string::npos) return false;  // every line ends with a newline
    const std::string line = bytes.substr(line_start, line_end - line_start);
    line_start = line_end + 1;
    if (header) { if (line != kHeader) return false; header = false; continue; }
    if (line.empty()) continue;
    // "<app> <layout> <revision>"
    const size_t a = line.find(' ');
    const size_t b = a == std::string::npos ? std::string::npos : line.find(' ', a + 1);
    if (a == std::string::npos || b == std::string::npos || line.find(' ', b + 1) != std::string::npos) return false;
    unsigned long long app = 0, revision = 0;
    if (!ParseNumber(line.substr(0, a), app) || !ParseNumber(line.substr(b + 1), revision) ||
        app == 0 || app > 0xffffffffull || revision == 0)
      return false;
    Entry entry;
    if (!ParseLayout(line.substr(a + 1, b - a - 1), entry.layout)) return false;
    entry.revision = revision;
    parsed[static_cast<std::uint32_t>(app)] = entry;
  }
  if (header) return false;
  entries_ = std::move(parsed);
  return true;
}

ControllerLayout ControllerLayoutStore::Get(std::uint32_t app) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = entries_.find(app);
  return found == entries_.end() ? ControllerLayout::gamepad : found->second.layout;
}

std::uint64_t ControllerLayoutStore::Set(std::uint32_t app, ControllerLayout layout) {
  if (!app) return 0;
  std::lock_guard<std::mutex> lock(mutex_);
  const auto previous = entries_;
  auto &entry = entries_[app];
  entry.layout = layout;
  entry.revision += 1;
  if (!SaveLocked()) { entries_ = previous; return 0; }
  return entry.revision;
}

std::optional<playden::runtime::GameSettingsRecord> ControllerLayoutStore::Record(std::uint32_t app) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = entries_.find(app);
  if (found == entries_.end() && app != kSkyrimSpecialEdition) return std::nullopt;
  playden::runtime::GameSettingsRecord record;
  if (found != entries_.end()) {
    record.overrides = ControllerLayoutOverrides(found->second.layout);
    record.revision = found->second.revision;
  }
  if (app == kSkyrimSpecialEdition) {
    if (found == entries_.end())
      record.overrides = ControllerLayoutOverrides(ControllerLayout::gamepad);
    record.overrides["program"] = kSkyrimProgram;
    record.overrides["launch_option"] = playden::runtime::kProgramEntryId;
    record.revision += kSkyrimSettingsEpoch;
  }
  // Playden republishes saved settings only when their revision is newer than
  // the prepared title's. A controller choice already supplies that revision.
  if (record.revision == 0) record.revision = 1;
  return record;
}

bool ControllerLayoutStore::SaveLocked() const {
  std::string bytes = std::string(kHeader) + "\n";
  for (const auto &[app, entry] : entries_)
    bytes += std::to_string(app) + " " + LayoutKey(entry.layout) + " " + std::to_string(entry.revision) + "\n";
  const auto target = folder_ / kFile;
  const auto temporary = folder_ / (std::string(".") + kFile + ".new");
  (void)unlink(temporary.c_str());
  const int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  bool ok = WriteAll(fd, bytes) && fsync(fd) == 0;
  if (close(fd) != 0) ok = false;
  if (!ok || rename(temporary.c_str(), target.c_str()) != 0) { (void)unlink(temporary.c_str()); return false; }
  const int dir = open(folder_.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dir >= 0) { (void)fsync(dir); close(dir); }
  return true;
}
}  // namespace ww::steam_integration
