// SPDX-License-Identifier: GPL-3.0-or-later
#include "uninstall_journal.hpp"
#include "uninstall_identity.hpp"
#include "uninstall_registry.hpp"
#include "platform/ps5/title_allow_list.h"
#include <charconv>
#include <array>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace ww::steam_integration {
namespace {
constexpr const char* kName = "vapor-uninstall.v1";
constexpr std::size_t kMaxRegistry = 128 * 1024;
std::string Hex(const std::string& bytes) {
  constexpr char digits[] = "0123456789abcdef"; std::string result; result.reserve(bytes.size() * 2);
  for (unsigned char c : bytes) { result.push_back(digits[c >> 4]); result.push_back(digits[c & 15]); }
  return result;
}
bool Unhex(const std::string& hex, std::string& bytes) {
  if (hex.empty() || hex.size() % 2 || hex.size() > kMaxRegistry * 2) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  bytes.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    int a = digit(hex[i]), b = digit(hex[i + 1]); if (a < 0 || b < 0) return false;
    bytes.push_back(static_cast<char>((a << 4) | b));
  }
  return true;
}
bool ValidTitle(const std::string& title) {
  if (title.size() != 9 || title.compare(0, 6, "FAKE78") != 0) return false;
  for (std::size_t i = 6; i < title.size(); ++i) if (title[i] < '0' || title[i] > '9') return false;
  return true;
}
bool Same(const UninstallJournal::Entry& a, const UninstallJournal::Entry& b) {
  return a.phase == b.phase && a.app == b.app && a.operation == b.operation && a.root_device == b.root_device &&
         a.root_inode == b.root_inode && a.title == b.title && a.reinstall == b.reinstall && a.account == b.account &&
         a.registry_before == b.registry_before;
}
playden::Bytes Encode(const UninstallJournal::Entry& e, std::uint64_t parent_device, std::uint64_t parent_inode) {
  const auto text = std::string("1 ") + std::to_string(static_cast<unsigned>(e.phase)) + " " +
      std::to_string(e.app) + " " + e.operation + " " + std::to_string(parent_device) + " " +
      std::to_string(parent_inode) + " " + std::to_string(e.root_device) + " " + std::to_string(e.root_inode) +
      " " + e.title + " " + (e.reinstall ? "1 " : "0 ") + std::to_string(e.account) + " " + Hex(e.registry_before) + "\n";
  return {text.begin(), text.end()};
}
}  // namespace
UninstallJournal::UninstallJournal(const playden::platform::ManagedDirectory& state,
    const playden::platform::ManagedFileLock& state_lock, const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& games_lock)
    : state_(state), state_lock_(state_lock), games_(games), games_lock_(games_lock), storage_(state.path().string()) {}
bool UninstallJournal::ParentIdentity(std::uint64_t& device, std::uint64_t& inode) const {
  struct stat st{};
  if (!state_.Validate() || !state_lock_.Validate(state_, "wine11-state.lock") || !games_.Validate() ||
      !games_lock_.Validate(games_, "wine11-install.lock") || fstat(games_.fd(), &st) || !S_ISDIR(st.st_mode)) return false;
  device = st.st_dev; inode = st.st_ino; return true;
}
UninstallJournal::Entry UninstallJournal::Read() const {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::uint64_t device = 0, inode = 0;
  if (!ParentIdentity(device, inode)) return {};
  const auto bytes = storage_.ReadDurable(kName, kMaxRegistry * 2 + 512);
  if (!bytes.ok()) { Entry e; if (bytes.error() == playden::StorageError::kNotFound) e.phase = Phase::absent; return e; }
  const std::string text(bytes.value().begin(), bytes.value().end());
  if (text.empty() || text.back() != '\n') return {};
  std::array<std::string_view, 12> fields{};
  std::string_view line(text.data(), text.size() - 1);
  for (std::size_t i = 0; i < fields.size(); ++i) {
    const auto pos = line.find(' ');
    if ((i + 1 == fields.size()) != (pos == std::string_view::npos)) return {};
    fields[i] = line.substr(0, pos);
    if (fields[i].empty()) return {};
    if (pos != std::string_view::npos) line.remove_prefix(pos + 1);
  }
  auto number = [](std::string_view field, auto& value) {
    const auto parsed = std::from_chars(field.data(), field.data() + field.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == field.data() + field.size();
  };
  unsigned version = 0, phase = 0, reinstall = 0; std::uint64_t pd = 0, pi = 0; Entry e;
  e.operation = fields[3]; e.title = fields[8];
  if (!number(fields[0], version) || !number(fields[1], phase) || !number(fields[2], e.app) ||
      !number(fields[4], pd) || !number(fields[5], pi) || !number(fields[6], e.root_device) ||
      !number(fields[7], e.root_inode) || !number(fields[9], reinstall) || !number(fields[10], e.account) ||
      version != 1 || phase < 1 || phase > 7 || !e.app || !ValidUninstallOperation(e.operation) || !e.root_inode ||
      pd != device || pi != inode || e.root_device != device || !ValidTitle(e.title) || reinstall > 1 || (reinstall && !e.account) ||
      !Unhex(std::string(fields[11]), e.registry_before) || !PlanInstalledRegistryRow(e.app, e.registry_before)) return {};
  e.phase = static_cast<Phase>(phase); e.reinstall = reinstall == 1;
  const auto encoded = Encode(e, device, inode);
  if (std::string(encoded.begin(), encoded.end()) != text) return {};
  return e;
}
bool UninstallJournal::Begin(const Entry& entry) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::uint64_t device = 0, inode = 0; struct stat root{};
  if (entry.phase != Phase::intent || !entry.app || !ValidUninstallOperation(entry.operation) || !ValidTitle(entry.title) || (entry.reinstall && !entry.account) ||
      entry.registry_before.size() > kMaxRegistry || !PlanInstalledRegistryRow(entry.app, entry.registry_before) ||
      Read().phase != Phase::absent || !ParentIdentity(device, inode) ||
      !games_.InspectChild(std::to_string(entry.app), &root) || !S_ISDIR(root.st_mode) ||
      root.st_dev != device || root.st_dev != entry.root_device || root.st_ino != entry.root_inode || root.st_uid != geteuid()) return false;
  const auto actual = ReadInstalledRegistry(games_, games_lock_, kMaxRegistry);
  if (!actual || *actual != entry.registry_before ||
      !ParentIdentity(device, inode)) return false;
  if (!storage_.WriteDurableExclusive(kName, Encode(entry, device, inode)).ok()) return false;
  return Same(Read(), entry);
}
bool UninstallJournal::Advance(const Entry& expected, Phase next) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  std::uint64_t device = 0, inode = 0;
  if (expected.phase < Phase::intent || expected.phase >= Phase::download_queued ||
      (expected.phase == Phase::quarantine_removed && !expected.reinstall) ||
      static_cast<unsigned>(next) != static_cast<unsigned>(expected.phase) + 1 ||
      !Same(Read(), expected) || !ParentIdentity(device, inode)) return false;
  Entry updated = expected; updated.phase = next;
  return storage_.WriteDurable(kName, Encode(updated, device, inode)).ok() && Same(Read(), updated);
}
bool UninstallJournal::Clear(const Entry& expected) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  if (!((expected.phase == Phase::quarantine_removed && !expected.reinstall) ||
        (expected.phase == Phase::download_queued && expected.reinstall)) || !Same(Read(), expected)) return false;
  return storage_.RemoveDurable(kName).ok() && Read().phase == Phase::absent;
}
}  // namespace ww::steam_integration

