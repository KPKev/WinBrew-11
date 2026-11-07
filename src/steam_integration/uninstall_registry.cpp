// SPDX-License-Identifier: GPL-3.0-or-later
#include "uninstall_registry.hpp"
#include "platform/ps5/title_allow_list.h"
#include "platform/storage.h"
#include "library/store.h"
#include <charconv>
#include <cerrno>
#include <set>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
namespace ww::steam_integration {
std::optional<std::string> ReadInstalledRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, std::size_t max_bytes) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto valid = [&] {
    return games.policy() == playden::platform::DirectoryPolicy::kPrivate &&
           games.Validate() && lease.Validate(games, "wine11-install.lock");
  };
  if (!max_bytes || max_bytes > 1024 * 1024 || !valid()) return {};
  const int fd = games.OpenRegularFile(".playden-games", true);
  if (fd < 0) return {};
  struct stat before{}, after{}, named{};
  bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 &&
      before.st_uid == geteuid() && before.st_size >= 0 &&
      static_cast<std::uint64_t>(before.st_size) <= max_bytes &&
      ((before.st_mode & 07777) == 0600 || (before.st_mode & 07777) == 0644 ||
       (before.st_mode & 07777) == 0666);
  std::string contents;
  if (ok) contents.resize(static_cast<std::size_t>(before.st_size));
  std::size_t offset = 0;
  while (ok && offset < contents.size()) {
    const auto n = read(fd, contents.data() + offset, contents.size() - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    offset += static_cast<std::size_t>(n);
  }
  ok = ok && fsync(fd) == 0 && fstat(fd, &after) == 0 &&
      before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
      before.st_uid == after.st_uid && before.st_mode == after.st_mode &&
      before.st_nlink == after.st_nlink && before.st_size == after.st_size &&
      before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
      before.st_ctim.tv_sec == after.st_ctim.tv_sec && before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
  const bool closed = close(fd) == 0;
  if (!ok || !closed || !games.Sync() || !valid() ||
      !games.InspectChild(".playden-games", &named) || named.st_dev != after.st_dev ||
      named.st_ino != after.st_ino || named.st_uid != after.st_uid || named.st_mode != after.st_mode ||
      named.st_nlink != after.st_nlink || named.st_size != after.st_size ||
      named.st_mtim.tv_sec != after.st_mtim.tv_sec || named.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
      named.st_ctim.tv_sec != after.st_ctim.tv_sec || named.st_ctim.tv_nsec != after.st_ctim.tv_nsec || !valid()) return {};
  return contents;
}
std::optional<RegistryRemovalPlan> PlanInstalledRegistryRow(std::uint32_t app, const std::string& original) {
  if (!app || original.size() > 1024 * 1024 || original.empty() || original.back() != '\n') return std::nullopt;
  const std::string key = std::to_string(app);
  std::string updated; updated.reserve(original.size()); unsigned matches = 0; std::set<std::string> seen;
  for (std::size_t pos = 0; pos < original.size();) {
    const auto end = original.find('\n', pos);
    const std::string_view line(original.data() + pos, end - pos);
    const auto first = line.find('\t');
    const auto id = line.substr(0, first);
    const auto parsed_id = playden::ParseKey(id);
    if (!parsed_id || playden::ToKey(*parsed_id) != id || !seen.insert(std::string(id)).second ||
        first == std::string_view::npos || line.size() >= 159) return std::nullopt;
    const auto second = line.find('\t', first + 1);
    if (second == std::string_view::npos || line.substr(first + 1, second - first - 1) != "internal") return std::nullopt;
    const auto amount = line.substr(second + 1); std::uint64_t bytes = 0;
    const auto parsed = std::from_chars(amount.data(), amount.data() + amount.size(), bytes);
    // Other paused downloads are retained byte-for-byte while their worker is stopped.
    const bool paused = amount == "0\tpaused";
    if (!paused && (amount.empty() || parsed.ec != std::errc{} || parsed.ptr != amount.data() + amount.size() || !bytes)) return std::nullopt;
    if (id == key) {
      if (paused) return std::nullopt;
      if (++matches != 1) return std::nullopt;
    } else updated.append(original, pos, end + 1 - pos);
    pos = end + 1;
  }
  if (!matches) return std::nullopt;
  return RegistryRemovalPlan{app, original, updated};
}
RegistryRemoval RemoveInstalledRegistryRow(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto valid = [&] { return games.Validate() && lease.Validate(games, "wine11-install.lock"); };
  const auto checked = PlanInstalledRegistryRow(plan.app, plan.before);
  if (!checked || checked->after != plan.after || !valid()) return RegistryRemoval::blocked;
  playden::Storage storage(games.path().string());
  const auto source = ReadInstalledRegistry(games, lease, 1024 * 1024);
  if (!source || !valid()) return RegistryRemoval::blocked;
  const std::string& observed_before = *source;
  if (observed_before == plan.after) return RegistryRemoval::already_absent;
  if (observed_before != plan.before) return RegistryRemoval::blocked;
  const playden::Bytes bytes(plan.after.begin(), plan.after.end());
  // A failure can follow a successful rename. Do not infer which version won.
  if (!storage.WriteDurable(".playden-games", bytes).ok()) return RegistryRemoval::indeterminate;
  const auto observed = ReadInstalledRegistry(games, lease, 1024 * 1024);
  if (!observed || *observed != plan.after || !valid()) return RegistryRemoval::indeterminate;
  return RegistryRemoval::removed;
}
ReinstallRegistry InspectReinstallRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto checked = PlanInstalledRegistryRow(plan.app, plan.before);
  if (!checked || checked->after != plan.after || !games.Validate() || !lease.Validate(games, "wine11-install.lock"))
    return ReinstallRegistry::blocked;
  const auto bytes = ReadInstalledRegistry(games, lease, 1024 * 1024);
  if (!bytes || !games.Validate() || !lease.Validate(games, "wine11-install.lock")) return ReinstallRegistry::blocked;
  const std::string& text = *bytes;
  if (text == plan.after) return ReinstallRegistry::absent;
  if (text.empty() || text.back() != '\n') return ReinstallRegistry::blocked;
  const std::string key = std::to_string(plan.app); std::string other; unsigned matches = 0;
  ReinstallRegistry state = ReinstallRegistry::blocked;
  for (std::size_t pos = 0; pos < text.size();) {
    const auto end = text.find('\n', pos); const std::string_view line(text.data() + pos, end - pos);
    const auto tab = line.find('\t');
    if (line.substr(0, tab) == key) {
      if (++matches != 1 || tab == std::string_view::npos) return ReinstallRegistry::blocked;
      const auto second = line.find('\t', tab + 1);
      if (second == std::string_view::npos || line.substr(tab + 1, second - tab - 1) != "internal") return ReinstallRegistry::blocked;
      auto value = line.substr(second + 1);
      if (value == "0" || value == "0\tpaused") state = ReinstallRegistry::queued;
      else {
        std::uint64_t size = 0; const auto parsed = std::from_chars(value.data(), value.data() + value.size(), size);
        if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !size) return ReinstallRegistry::blocked;
        state = ReinstallRegistry::installed;
      }
    } else other.append(text, pos, end + 1 - pos);
    pos = end + 1;
  }
  return matches == 1 && other == plan.after ? state : ReinstallRegistry::blocked;
}
QueueReinstall QueueReinstallRegistry(const playden::platform::ManagedDirectory& games,
    const playden::platform::ManagedFileLock& lease, const RegistryRemovalPlan& plan) {
  const auto uid = playden::ps5::HoldEffectiveUid();
  const auto checked = PlanInstalledRegistryRow(plan.app, plan.before);
  if (!checked || checked->after != plan.after || !games.Validate() || !lease.Validate(games, "wine11-install.lock"))
    return QueueReinstall::blocked;
  playden::Storage storage(games.path().string());
  const auto current = ReadInstalledRegistry(games, lease, 1024 * 1024);
  if (!current || !games.Validate() || !lease.Validate(games, "wine11-install.lock")) return QueueReinstall::blocked;
  const std::string& text = *current;
  const std::string queued = plan.after + std::to_string(plan.app) + "\tinternal\t0\n";
  if (text == queued) return QueueReinstall::already_queued;
  if (text != plan.after) return QueueReinstall::blocked;
  const playden::Bytes bytes(queued.begin(), queued.end());
  if (!storage.WriteDurable(".playden-games", bytes).ok()) return QueueReinstall::indeterminate;
  const auto observed = ReadInstalledRegistry(games, lease, 1024 * 1024);
  if (!observed || *observed != queued || !games.Validate() || !lease.Validate(games, "wine11-install.lock"))
    return QueueReinstall::indeterminate;
  return QueueReinstall::queued;
}
}
